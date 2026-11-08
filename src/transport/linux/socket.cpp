#include "gateway/transport/socket.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

#include <netdb.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

// Maps the socket protocol to the corresponding POSIX socket type.
int socketType(Socket::Protocol protocol)
{
    return protocol == Socket::Protocol::UDP ? SOCK_DGRAM : SOCK_STREAM;
}

// Maps the socket protocol to the corresponding IP transport protocol.
int transportProtocol(Socket::Protocol protocol)
{
    return protocol == Socket::Protocol::UDP ? IPPROTO_UDP : IPPROTO_TCP;
}

// Receives exactly one UDP datagram into packet.data and records its sender in packet.peer.
// Returns the datagram size, or 0 on timeout, error, or maxSize == 0.
// A datagram longer than maxSize is truncated and the excess bytes are discarded.
std::size_t receiveDatagram(int fd, Packet& packet, int maxSize, int timeoutMs)
{
    // A zero-length request never consumes a datagram.
    if (maxSize == 0)
        return 0;

    // Wait for the socket to become readable; a negative timeoutMs waits indefinitely.
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ready;
    do {
        ready = ::poll(&pfd, 1, timeoutMs);
    } while (ready < 0 && errno == EINTR);

    // Timeout or poll failure: no datagram was consumed.
    if (ready <= 0 || !(pfd.revents & (POLLIN | POLLERR)))
        return 0;

    // Use the caller's limit if given; otherwise the maximum UDP payload size.
    const std::size_t capacity = maxSize < 0 ? 65535 : static_cast<std::size_t>(maxSize);
    packet.data.resize(capacity);

    // Storage for the sender address, filled by recvfrom().
    struct sockaddr_storage from;
    socklen_t fromLength = sizeof(from);

    // recvfrom() returns one whole datagram; retry only if interrupted by a signal.
    ssize_t received;
    do {
        received = ::recvfrom(fd, packet.data.data(), capacity, 0,
            reinterpret_cast<struct sockaddr*>(&from), &fromLength);
    } while (received < 0 && errno == EINTR);

    if (received < 0) {
        packet.data.clear();
        return 0;
    }

    // Shrink the buffer to the datagram size actually received.
    packet.data.resize(static_cast<std::size_t>(received));

    // Convert the sender address to numeric host and port strings.
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    if (::getnameinfo(reinterpret_cast<struct sockaddr*>(&from), fromLength,
            host, sizeof(host), service, sizeof(service),
            NI_NUMERICHOST | NI_NUMERICSERV)
        == 0) {
        packet.peer.host = host;
        packet.peer.port = service;
    }

    return static_cast<std::size_t>(received);
}

} // namespace

// Holds the OS-specific state of the socket: the file descriptor only.
struct Socket::SocketHandle {
    int fd = -1;
};

Socket::Socket()
    : Socket(Config {})
{
}

Socket::Socket(const Config& config)
    : config_(config)
    , handle_(std::make_unique<SocketHandle>())
{
}

Socket::~Socket()
{
    // Release the file descriptor if it is still open.
    close();
}

bool Socket::open()
{
    // Drop any previous connection before opening a new one.
    close();

    // Resolve the host/port pair for either IPv4 or IPv6.
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = socketType(config_.protocol);
    hints.ai_protocol = transportProtocol(config_.protocol);

    struct addrinfo* results = nullptr;
    if (::getaddrinfo(config_.host.c_str(), config_.port.c_str(), &hints, &results) != 0)
        return false;

    // Try each resolved address until one socket can be created and connected.
    // For UDP, connect() only records the default destination, so it succeeds
    // as soon as the address is valid.
    int fd = -1;
    for (struct addrinfo* address = results; address != nullptr; address = address->ai_next) {
        fd = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0)
            continue;

        if (::connect(fd, address->ai_addr, address->ai_addrlen) == 0)
            break;

        ::close(fd);
        fd = -1;
    }

    ::freeaddrinfo(results);

    // No address could be connected, so report failure.
    if (fd < 0)
        return false;

    handle_->fd = fd;
    return true;
}

void Socket::close()
{
    // Nothing to do if the socket is already closed.
    if (!handle_ || handle_->fd < 0)
        return;

    ::close(handle_->fd);
    handle_->fd = -1;
}

bool Socket::isOpen() const
{
    return handle_ && handle_->fd >= 0;
}

std::size_t Socket::receive(Packet& packet, int maxSize, int timeoutMs)
{
    if (!isOpen())
        return 0;

    auto& buffer = packet.data;
    buffer.clear();

    if (config_.protocol == Protocol::UDP)
        return receiveDatagram(handle_->fd, packet, maxSize, timeoutMs);

    // Determine how many bytes to read: either everything currently available
    // (maxSize < 0) or at most maxSize bytes.
    const bool readAvailable = maxSize < 0;
    std::size_t limit;

    if (readAvailable) {
        int available = 0;
        if (::ioctl(handle_->fd, FIONREAD, &available) != 0 || available <= 0)
            return 0;
        limit = static_cast<std::size_t>(available);
    } else {
        if (maxSize == 0)
            return 0;
        limit = static_cast<std::size_t>(maxSize);
    }

    // A timeout only applies when reading a bounded amount of data.
    const bool hasTimeout = !readAvailable && timeoutMs > 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    buffer.resize(limit);

    // Read until the requested amount is collected or the socket times out/closes.
    std::size_t total = 0;
    while (total < limit) {
        // Wait indefinitely for bounded reads; do not wait when draining
        // already-available data.
        int waitMs = readAvailable ? 0 : -1;

        if (hasTimeout) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
                break;
            waitMs = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        }

        // Wait for the socket to become readable.
        struct pollfd pfd;
        pfd.fd = handle_->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ready;
        do {
            ready = ::poll(&pfd, 1, waitMs);
        } while (ready < 0 && errno == EINTR);

        // Timeout, error, or no relevant event: stop reading.
        if (ready <= 0)
            break;
        if (!(pfd.revents & (POLLIN | POLLHUP | POLLERR)))
            break;

        // Receive the next chunk into the remaining space of the buffer.
        ssize_t received;
        do {
            received = ::recv(handle_->fd, buffer.data() + total, limit - total, 0);
        } while (received < 0 && errno == EINTR);

        if (received < 0) {
            // Spurious wakeup: wait again.
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            break;
        }
        // Orderly shutdown (TCP) or no more data.
        if (received == 0)
            break;

        total += static_cast<std::size_t>(received);
    }

    // Shrink the buffer to the number of bytes actually read.
    buffer.resize(total);
    return total;
}

std::size_t Socket::send(const Packet& packet)
{
    const auto& buffer = packet.data;

    // Nothing to send on a closed socket or with an empty buffer.
    if (!isOpen() || buffer.empty())
        return 0;

    // Keep sending until the whole buffer is written or an error occurs.
    std::size_t written = 0;
    while (written < buffer.size()) {
        // MSG_NOSIGNAL prevents SIGPIPE when the peer has closed the connection.
        ssize_t result = ::send(handle_->fd, buffer.data() + written, buffer.size() - written, MSG_NOSIGNAL);

        if (result < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (result == 0)
            break;

        written += static_cast<std::size_t>(result);
    }

    return written;
}

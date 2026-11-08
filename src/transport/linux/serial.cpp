#include "gateway/transport/serial.hpp"

// Standard library helpers: timeouts and error handling.
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <utility>

// POSIX/Linux serial port primitives.
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace {

// Translate a numeric baud rate into the platform's speed_t constant.
bool toSpeed(unsigned int baudRate, speed_t& speed)
{
    switch (baudRate) {
    case 1200:
        speed = B1200;
        break;
    case 2400:
        speed = B2400;
        break;
    case 4800:
        speed = B4800;
        break;
    case 9600:
        speed = B9600;
        break;
    case 19200:
        speed = B19200;
        break;
    case 38400:
        speed = B38400;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
    case 230400:
        speed = B230400;
        break;
#ifdef B460800
    case 460800:
        speed = B460800;
        break;
#endif
#ifdef B921600
    case 921600:
        speed = B921600;
        break;
#endif
    default:
        // Unsupported rates are rejected by the caller.
        return false;
    }
    return true;
}

} // namespace

// Linux-specific state hidden behind the public Serial class.
struct Serial::SerialHandle {
    int fd = -1; // File descriptor for the opened tty, or -1 when closed.
};

// Allocate the opaque handle; the port stays closed until open() succeeds.
Serial::Serial()
    : Serial(Config{})
{
}

Serial::Serial(Config config)
    : config_(std::move(config))
    , handle_(std::make_unique<SerialHandle>())
{
}

// Ensure the descriptor is released when the object is destroyed.
Serial::~Serial()
{
    close();
}

// Open and configure a tty device with the requested line settings.
bool Serial::open()
{
    close();

    speed_t speed = B9600;
    if (!toSpeed(config_.baudRate, speed))
        return false;

    int fd = ::open(config_.port.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0)
        return false;

    // Fetch the current line settings so we can modify only what we need.
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        ::close(fd);
        return false;
    }

    // Start from raw mode: no echo, no line buffering, no signal generation.
    cfmakeraw(&tty);

    // Apply the requested input and output baud rate.
    if (cfsetspeed(&tty, speed) != 0) {
        ::close(fd);
        return false;
    }

    // Enable the receiver, ignore modem control lines, and use 8 data bits.
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    // Configure parity: disabled, odd, or even.
    switch (config_.parity) {
    case Parity::NONE:
        tty.c_cflag &= ~PARENB;
        break;
    case Parity::ODD:
        tty.c_cflag |= PARENB;
        tty.c_cflag |= PARODD;
        break;
    case Parity::EVEN:
        tty.c_cflag |= PARENB;
        tty.c_cflag &= ~PARODD;
        break;
    }

    // Configure one or two stop bits.
    if (config_.stopBits == StopBits::TWO)
        tty.c_cflag |= CSTOPB;
    else
        tty.c_cflag &= ~CSTOPB;

    // Blocking reads return as soon as at least one byte is available.
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    // Commit the new line settings; on failure leave the port closed.
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        ::close(fd);
        return false;
    }

    // Discard any stale input/output buffered before configuration.
    tcflush(fd, TCIOFLUSH);

    // Publish the successfully configured descriptor.
    handle_->fd = fd;
    return true;
}

// Release the underlying descriptor if the port is open.
void Serial::close()
{
    if (!handle_ || handle_->fd < 0)
        return;

    ::close(handle_->fd);
    handle_->fd = -1;
}

// A valid descriptor is the single source of truth for the open state.
bool Serial::isOpen() const
{
    return handle_ && handle_->fd >= 0;
}

// Read from the port into buffer according to maxSize and timeoutMs.
std::size_t Serial::receive(Packet& packet, int maxSize, int timeoutMs)
{
    auto& buffer = packet.data;

    // Nothing to read when the port is not open.
    if (!isOpen())
        return 0;

    // buffer is an output parameter; discard any caller-provided contents.
    buffer.clear();

    // A negative maxSize means "read what is currently available".
    const bool readAvailable = maxSize < 0;
    std::size_t limit;

    if (readAvailable) {
        // Ask the driver how many bytes are queued right now.
        int available = 0;
        if (::ioctl(handle_->fd, FIONREAD, &available) != 0 || available <= 0)
            return 0;
        limit = static_cast<std::size_t>(available);
    } else {
        // A zero limit is a no-op; otherwise read at most maxSize bytes.
        if (maxSize == 0)
            return 0;
        limit = static_cast<std::size_t>(maxSize);
    }

    // Timeouts apply only to bounded reads with a positive timeout.
    const bool hasTimeout = !readAvailable && timeoutMs > 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    // Reserve exactly the number of bytes we intend to return at most.
    buffer.resize(limit);

    std::size_t total = 0;
    while (total < limit) {
        // Available reads use zero-timeout polls; bounded reads block by default.
        int waitMs = readAvailable ? 0 : -1;

        // For bounded reads with a timeout, wait only until the shared deadline.
        if (hasTimeout) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
                break;
            waitMs = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        }

        // Wait for input, a hang-up, or an error on the serial descriptor.
        struct pollfd pfd;
        pfd.fd = handle_->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ready;
        do {
            ready = ::poll(&pfd, 1, waitMs);
        } while (ready < 0 && errno == EINTR);

        // Timeout or unrecoverable poll failure ends the read.
        if (ready <= 0)
            break;
        if (!(pfd.revents & (POLLIN | POLLHUP | POLLERR)))
            break;

        // Consume whatever the driver has ready, up to the remaining limit.
        ssize_t received;
        do {
            received = ::read(handle_->fd, buffer.data() + total, limit - total);
        } while (received < 0 && errno == EINTR);

        if (received < 0) {
            // Non-blocking descriptors may race with poll(); retry or stop.
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            break;
        }
        // EOF/hang-up: no more data will arrive for this read call.
        if (received == 0)
            break;

        total += static_cast<std::size_t>(received);
    }

    // Shrink the output buffer to the amount actually received.
    buffer.resize(total);
    return total;
}

// Write the entire buffer, handling partial writes and interruptions.
std::size_t Serial::send(const Packet& packet)
{
    const auto& buffer = packet.data;

    if (!isOpen() || buffer.empty())
        return 0;

    std::size_t written = 0;
    while (written < buffer.size()) {
        ssize_t result = ::write(handle_->fd, buffer.data() + written, buffer.size() - written);

        // Retry syscalls interrupted by a signal.
        if (result < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        // A zero-byte write makes no progress; stop to avoid spinning.
        if (result == 0)
            break;

        written += static_cast<std::size_t>(result);
    }

    // Return the number of bytes successfully queued.
    return written;
}

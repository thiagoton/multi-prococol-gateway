#include "gateway/transport/can.hpp"

// Standard library helpers: error handling and byte limits.
#include <algorithm>
#include <cerrno>
#include <cstring>

// Linux SocketCAN primitives.
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

// Largest payload carried by a classic CAN frame.
constexpr std::size_t kMaxPayload = CAN_MAX_DLEN;

} // namespace

// Linux-specific state hidden behind the public Can class.
struct Can::CanHandle {
    int fd = -1; // SocketCAN descriptor, or -1 when closed.
};

// Allocate the opaque handle; the interface stays closed until open() succeeds.
Can::Can()
    : Can(Config {})
{
}

Can::Can(const Config& config)
    : config_(config)
    , handle_(std::make_unique<CanHandle>())
{
}

// Ensure the descriptor is released when the object is destroyed.
Can::~Can()
{
    close();
}

// Open a raw CAN socket bound to the configured network interface.
//
// The interface bitrate is a property of the network device rather than the
// socket, so it must be configured on the system (for example with
// `ip link set <interface> type can bitrate <bitrate>`). config_.bitrate is
// kept so callers can describe the expected link settings.
bool Can::open()
{
    close();

    // A zero interface name can never resolve to a valid device.
    if (config_.interface.empty())
        return false;

    unsigned int index = ::if_nametoindex(config_.interface.c_str());
    if (index == 0)
        return false;

    int fd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0)
        return false;

    // Bind the socket to the resolved CAN interface index.
    struct sockaddr_can address;
    std::memset(&address, 0, sizeof(address));
    address.can_family = AF_CAN;
    address.can_ifindex = static_cast<int>(index);

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(fd);
        return false;
    }

    // Publish the successfully bound descriptor.
    handle_->fd = fd;
    return true;
}

// Release the underlying descriptor if the interface is open.
void Can::close()
{
    if (!handle_ || handle_->fd < 0)
        return;

    ::close(handle_->fd);
    handle_->fd = -1;
}

// A valid descriptor is the single source of truth for the open state.
bool Can::isOpen() const
{
    return handle_ && handle_->fd >= 0;
}

// Read one CAN frame into the packet according to maxSize and timeoutMs.
std::size_t Can::receive(Packet& packet, int maxSize, int timeoutMs)
{
    auto& buffer = packet.data;

    // Nothing to read when the interface is not open.
    if (!isOpen())
        return 0;

    // buffer is an output parameter; discard any caller-provided contents.
    buffer.clear();

    // A zero limit is a no-op.
    if (maxSize == 0)
        return 0;

    // Wait for a frame; a non-positive timeout waits indefinitely.
    const int waitMs = timeoutMs > 0 ? timeoutMs : -1;

    struct pollfd pfd;
    pfd.fd = handle_->fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ready;
    do {
        ready = ::poll(&pfd, 1, waitMs);
    } while (ready < 0 && errno == EINTR);

    // Timeout, poll failure, or no readable event: no frame received.
    if (ready <= 0 || !(pfd.revents & (POLLIN | POLLERR)))
        return 0;

    // Read a single frame; retry only if interrupted by a signal.
    struct can_frame frame;
    std::memset(&frame, 0, sizeof(frame));

    ssize_t received;
    do {
        received = ::read(handle_->fd, &frame, sizeof(frame));
    } while (received < 0 && errno == EINTR);

    if (received < static_cast<ssize_t>(sizeof(frame)))
        return 0;

    // The identifier carries the frame id along with the EFF/RTR/ERR flags.
    packet.id = frame.can_id;

    // Copy the payload, honoring the caller's limit when positive.
    const std::size_t length = std::min<std::size_t>(frame.can_dlc, kMaxPayload);
    const std::size_t limit = maxSize < 0 ? length : std::min<std::size_t>(length, static_cast<std::size_t>(maxSize));

    buffer.assign(frame.data, frame.data + limit);
    return limit;
}

// Write one CAN frame using packet.id as the frame identifier.
std::size_t Can::send(const Packet& packet)
{
    const auto& buffer = packet.data;

    // Nothing to send on a closed interface or with an empty buffer.
    if (!isOpen() || buffer.empty())
        return 0;

    struct can_frame frame;
    std::memset(&frame, 0, sizeof(frame));

    frame.can_id = packet.id;
    frame.can_dlc = static_cast<__u8>(std::min<std::size_t>(buffer.size(), kMaxPayload));
    std::copy(buffer.begin(), buffer.begin() + frame.can_dlc, frame.data);

    ssize_t result;
    do {
        result = ::write(handle_->fd, &frame, sizeof(frame));
    } while (result < 0 && errno == EINTR);

    if (result != static_cast<ssize_t>(sizeof(frame)))
        return 0;

    // Return the number of payload bytes successfully queued.
    return frame.can_dlc;
}

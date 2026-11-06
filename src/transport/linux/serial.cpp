#include "gateway/transport/serial.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace {

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
        return false;
    }
    return true;
}

} // namespace

struct Serial::SerialHandle {
    int fd = -1;
};

Serial::Serial()
    : handle_(std::make_unique<SerialHandle>())
{
}

Serial::~Serial()
{
    close();
}

bool Serial::open(const std::string& port, unsigned int baudRate, Parity parity, StopBits stopBits)
{
    close();

    speed_t speed = B9600;
    if (!toSpeed(baudRate, speed))
        return false;

    int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0)
        return false;

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        ::close(fd);
        return false;
    }

    cfmakeraw(&tty);

    if (cfsetspeed(&tty, speed) != 0) {
        ::close(fd);
        return false;
    }

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    switch (parity) {
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

    if (stopBits == StopBits::TWO)
        tty.c_cflag |= CSTOPB;
    else
        tty.c_cflag &= ~CSTOPB;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        ::close(fd);
        return false;
    }

    tcflush(fd, TCIOFLUSH);

    handle_->fd = fd;
    return true;
}

void Serial::close()
{
    if (!handle_ || handle_->fd < 0)
        return;

    ::close(handle_->fd);
    handle_->fd = -1;
}

bool Serial::isOpen() const
{
    return handle_ && handle_->fd >= 0;
}

std::size_t Serial::read(std::vector<std::uint8_t>& buffer, int maxSize, int timeoutMs)
{
    if (!isOpen())
        return 0;

    buffer.clear();

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

    const bool hasTimeout = !readAvailable && timeoutMs > 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    buffer.resize(limit);

    std::size_t total = 0;
    while (total < limit) {
        int waitMs = readAvailable ? 0 : -1;

        if (hasTimeout) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
                break;
            waitMs = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        }

        struct pollfd pfd;
        pfd.fd = handle_->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ready;
        do {
            ready = ::poll(&pfd, 1, waitMs);
        } while (ready < 0 && errno == EINTR);

        if (ready <= 0)
            break;
        if (!(pfd.revents & (POLLIN | POLLHUP | POLLERR)))
            break;

        ssize_t received;
        do {
            received = ::read(handle_->fd, buffer.data() + total, limit - total);
        } while (received < 0 && errno == EINTR);

        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            break;
        }
        if (received == 0)
            break;

        total += static_cast<std::size_t>(received);
    }

    buffer.resize(total);
    return total;
}

std::size_t Serial::write(std::vector<std::uint8_t>& buffer)
{
    if (!isOpen() || buffer.empty())
        return 0;

    std::size_t written = 0;
    while (written < buffer.size()) {
        ssize_t result = ::write(handle_->fd, buffer.data() + written, buffer.size() - written);

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

#pragma once

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace support {

class FileDescriptor {
public:
    FileDescriptor() = default;

    explicit FileDescriptor(int fd)
        : fd_(fd)
    {
    }

    ~FileDescriptor()
    {
        reset();
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    FileDescriptor(FileDescriptor&& other) noexcept
        : fd_(other.release())
    {
    }

    FileDescriptor& operator=(FileDescriptor&& other) noexcept
    {
        if (this != &other)
            reset(other.release());
        return *this;
    }

    int get() const
    {
        return fd_;
    }

    explicit operator bool() const
    {
        return fd_ >= 0;
    }

    int release()
    {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void reset(int fd = -1)
    {
        if (fd_ >= 0)
            ::close(fd_);
        fd_ = fd;
    }

private:
    int fd_ = -1;
};

inline bool waitReadable(int fd, int timeoutMs)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ready;
    do {
        ready = ::poll(&pfd, 1, timeoutMs);
    } while (ready < 0 && errno == EINTR);

    return ready > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR)) != 0;
}

inline std::vector<std::uint8_t> readExactly(int fd, std::size_t count, int timeoutMs)
{
    std::vector<std::uint8_t> bytes;
    std::uint8_t chunk[256];

    while (bytes.size() < count && waitReadable(fd, timeoutMs)) {
        const std::size_t wanted = std::min(count - bytes.size(), sizeof(chunk));
        const ssize_t received = ::read(fd, chunk, wanted);

        if (received < 0 && errno == EINTR)
            continue;
        if (received <= 0)
            break;

        bytes.insert(bytes.end(), chunk, chunk + received);
    }

    return bytes;
}

inline bool writeAll(int fd, const std::vector<std::uint8_t>& bytes)
{
    std::size_t written = 0;
    while (written < bytes.size()) {
        const ssize_t result = ::write(fd, bytes.data() + written, bytes.size() - written);

        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            return false;

        written += static_cast<std::size_t>(result);
    }
    return true;
}

}

#pragma once

#include "support/fd.hpp"

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/socket.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace support {

inline bool canInterfaceExists(const std::string& name)
{
    return ::if_nametoindex(name.c_str()) != 0;
}

class CanPeer {
public:
    explicit CanPeer(const std::string& interface)
    {
        FileDescriptor fd(::socket(PF_CAN, SOCK_RAW, CAN_RAW));
        if (!fd)
            throw std::runtime_error("socket(PF_CAN) failed");

        struct sockaddr_can address;
        std::memset(&address, 0, sizeof(address));
        address.can_family = AF_CAN;
        address.can_ifindex = static_cast<int>(::if_nametoindex(interface.c_str()));

        if (::bind(fd.get(), reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0)
            throw std::runtime_error("bind() to CAN interface failed");

        fd_ = std::move(fd);
    }

    bool send(std::uint32_t id, const std::vector<std::uint8_t>& data)
    {
        struct can_frame frame;
        std::memset(&frame, 0, sizeof(frame));
        frame.can_id = id;
        frame.can_dlc = static_cast<__u8>(std::min<std::size_t>(data.size(), CAN_MAX_DLEN));
        std::copy(data.begin(), data.begin() + frame.can_dlc, frame.data);

        const ssize_t written = ::write(fd_.get(), &frame, sizeof(frame));
        return written == static_cast<ssize_t>(sizeof(frame));
    }

    bool receive(struct can_frame& frame, int timeoutMs)
    {
        if (!waitReadable(fd_.get(), timeoutMs))
            return false;

        const ssize_t received = ::read(fd_.get(), &frame, sizeof(frame));
        return received == static_cast<ssize_t>(sizeof(frame));
    }

private:
    FileDescriptor fd_;
};

}

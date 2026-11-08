#pragma once

#include "support/fd.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace support {

struct Datagram {
    std::vector<std::uint8_t> bytes;
    sockaddr_in from {};
};

inline FileDescriptor bindLoopback(int type, std::uint16_t& port)
{
    FileDescriptor fd(::socket(AF_INET, type, 0));
    if (!fd)
        throw std::runtime_error("socket() failed");

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (::bind(fd.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
        throw std::runtime_error("bind() failed");

    socklen_t length = sizeof(address);
    if (::getsockname(fd.get(), reinterpret_cast<sockaddr*>(&address), &length) != 0)
        throw std::runtime_error("getsockname() failed");

    port = ntohs(address.sin_port);
    return fd;
}

inline FileDescriptor listenTcp(std::uint16_t& port)
{
    FileDescriptor fd = bindLoopback(SOCK_STREAM, port);
    if (::listen(fd.get(), 4) != 0)
        throw std::runtime_error("listen() failed");
    return fd;
}

inline std::uint16_t unusedTcpPort()
{
    std::uint16_t port = 0;
    bindLoopback(SOCK_STREAM, port);
    return port;
}

inline bool receiveDatagram(int fd, int timeoutMs, Datagram& datagram)
{
    if (!waitReadable(fd, timeoutMs))
        return false;

    datagram.bytes.resize(2048);
    socklen_t length = sizeof(datagram.from);
    const ssize_t received = ::recvfrom(fd, datagram.bytes.data(), datagram.bytes.size(), 0,
        reinterpret_cast<sockaddr*>(&datagram.from), &length);
    if (received < 0)
        return false;

    datagram.bytes.resize(static_cast<std::size_t>(received));
    return true;
}

inline bool sendDatagram(int fd, const sockaddr_in& to, const std::vector<std::uint8_t>& bytes)
{
    const ssize_t sent = ::sendto(fd, bytes.data(), bytes.size(), 0,
        reinterpret_cast<const sockaddr*>(&to), sizeof(to));
    return sent == static_cast<ssize_t>(bytes.size());
}

}

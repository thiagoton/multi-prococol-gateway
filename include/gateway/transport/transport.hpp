#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct Endpoint {
    std::string host;
    std::string port;
};

struct Packet {
    std::vector<std::uint8_t> data;
    std::uint32_t id = 0;
    Endpoint peer;
};

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual std::size_t send(const Packet& packet) = 0;
    virtual std::size_t receive(Packet& packet, int maxSize, int timeoutMs) = 0;
};

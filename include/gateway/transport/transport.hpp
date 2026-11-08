#pragma once

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

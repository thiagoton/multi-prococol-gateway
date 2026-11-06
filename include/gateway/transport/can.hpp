#pragma once

#include <memory>
#include <string>

class Can {
public:
    Can();
    ~Can();

    bool open(const std::string& interface, unsigned int bitrate);
    void close();
    bool isOpen() const;
    bool send(uint32_t id, const uint8_t* data, std::size_t size);
    bool receive(uint32_t& id, uint8_t* data, std::size_t size, unsigned int timeoutMs);

protected:
    struct CanHandle;
    std::unique_ptr<CanHandle> handle_;
};
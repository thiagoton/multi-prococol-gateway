#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Serial {
public:
    enum Parity {
        NONE = 0,
        ODD,
        EVEN,
    };

    enum StopBits {
        ONE = 0,
        TWO,
    };

    Serial();
    ~Serial();

    bool open(const std::string& port, unsigned int baudRate, Parity parity = Parity::NONE, StopBits stopBits = StopBits::ONE);
    void close();
    bool isOpen() const;
    std::size_t read(std::vector<std::uint8_t>& buffer, int maxSize = -1, int timeoutMs = -1);
    std::size_t write(std::vector<std::uint8_t>& buffer);

protected:
    struct SerialHandle;
    std::unique_ptr<SerialHandle> handle_;
};
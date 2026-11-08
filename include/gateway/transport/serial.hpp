#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "gateway/transport/transport.hpp"

class Serial : public ITransport {
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

    struct Config {
        std::string port;
        unsigned int baudRate = 9600;
        Parity parity = Parity::NONE;
        StopBits stopBits = StopBits::ONE;
    };

    /**
     * Constructs a closed serial port object.
     */
    Serial();
    explicit Serial(const Config& config);

    /**
     * Closes the port if it is still open and releases the OS handle.
     */
    ~Serial();

    /**
     * Opens the serial port using its configured settings.
     *
     * @return True if the port was successfully opened, false otherwise.
     */
    bool open() override;

    /**
     * Closes the serial port and releases the underlying OS handle.
     * Calling close() on an already closed port has no effect.
     */
    void close() override;

    /**
     * Returns true if the serial port is currently opened.
     *
     * @return True if the port is open, false otherwise.
     */
    bool isOpen() const override;

    /**
     * Receives data from the serial port into packet.data. The packet data is
     * replaced with the bytes actually received.
     *
     * @param packet The packet that receives the data.
     * @param maxSize The maximum number of bytes to receive. If negative, receives all
     *                bytes currently available on the port (default: -1).
     * @param timeoutMs The maximum time in milliseconds to wait for maxSize bytes
     *                  when maxSize is positive. If zero or negative, waits
     *                  indefinitely (default: -1).
     * @return The number of bytes actually received.
     */
    std::size_t receive(Packet& packet, int maxSize = -1, int timeoutMs = -1) override;

    /**
     * Sends the packet data through the serial port.
     *
     * @param packet The packet containing the data to send.
     * @return The number of bytes actually sent.
     */
    std::size_t send(const Packet& packet) override;

private:
    Config config_;

protected:
    struct SerialHandle;
    std::unique_ptr<SerialHandle> handle_;
};


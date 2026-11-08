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

    /**
     * Constructs a closed serial port object.
     */
    Serial();

    /**
     * Closes the port if it is still open and releases the OS handle.
     */
    ~Serial();

    /**
     * Opens the serial port with the specified configuration.
     * 
     * @param port The port name (e.g., COM3 on Windows or /dev/ttyUSB0 on Unix).
     * @param baudRate The baud rate for communication.
     * @param parity The parity mode (default: NONE).
     * @param stopBits The number of stop bits (default: ONE).
     * @return True if the port was successfully opened, false otherwise.
     */
    bool open(const std::string& port, unsigned int baudRate, Parity parity = Parity::NONE, StopBits stopBits = StopBits::ONE);

    /**
     * Closes the serial port and releases the underlying OS handle.
     * Calling close() on an already closed port has no effect.
     */
    void close();

    /**
     * Returns true if the serial port is currently opened.
     *
     * @return True if the port is open, false otherwise.
     */
    bool isOpen() const;

    /**
     * Reads data from the serial port into the provided buffer.
     * The buffer is treated as an output parameter and is replaced with the
     * bytes actually read.
     *
     * @param buffer The buffer to store the read data.
     * @param maxSize The maximum number of bytes to read. If negative, reads all
     *                bytes currently available on the port (default: -1).
     * @param timeoutMs The maximum time in milliseconds to wait for maxSize bytes
     *                  when maxSize is positive. If zero or negative, waits
     *                  indefinitely (default: -1).
     * @return The number of bytes actually read.
     */
    std::size_t read(std::vector<std::uint8_t>& buffer, int maxSize = -1, int timeoutMs = -1);
    
    /**
     * Writes data to the serial port from the provided buffer.
     *
     * @param buffer The buffer containing the data to write.
     * @return The number of bytes actually written.
     */
    std::size_t write(std::vector<std::uint8_t>& buffer);

protected:
    struct SerialHandle;
    std::unique_ptr<SerialHandle> handle_;
};
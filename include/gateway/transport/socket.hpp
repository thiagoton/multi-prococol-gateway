#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Socket {
public:
    enum Protocol {
        TCP = 0,
        UDP,
    };

    struct Config {
        std::string host;
        std::string port;
        Protocol protocol = Protocol::TCP;
    };

    /**
     * Constructs a closed socket object.
     */
    Socket();

    /**
     * Constructs a closed socket object with the given configuration.
     *
     * @param config The host, port and protocol used by open().
     */
    explicit Socket(Config config);
    ~Socket();

    /**
     * Opens the socket to the configured host and port. For UDP, it sets
     * destination to send datagrams, so it always succeeds.
     * For TCP, it effectively connects to the host, so if connection fails, it returns false.
     *
     * @return True if the socket was successfully opened, false otherwise.
     */
    bool open();

    /**
     * Closes the socket and releases all resources.
     */
    void close();

    /**
     * Returns true if the socket is currently opened.
     *
     * @return True if the socket is open, false otherwise.
     */
    bool isOpen() const;

    /**
     * Reads data from the socket into the provided buffer.
     *
     * @param buffer The buffer to store the read data.
     * @param maxSize The maximum number of bytes to read (default: -1 for all available data).
     * @param timeoutMs The timeout in milliseconds used when maxSize > 0 (default: -1 for no timeout).
     * @return The number of bytes actually read.
     */
    std::size_t read(std::vector<std::uint8_t>& buffer, int maxSize = -1, int timeoutMs = -1);

    /**
     * Writes data to the socket from the provided buffer.
     *
     * @param buffer The buffer containing the data to write.
     * @return The number of bytes actually written.
     */
    std::size_t write(std::vector<std::uint8_t>& buffer);

protected:
    Config config_;
    // holds OS specific data
    struct SocketHandle;
    std::unique_ptr<SocketHandle> handle_;
};
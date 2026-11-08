#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "gateway/transport/transport.hpp"

class Socket : public ITransport {
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
    explicit Socket(const Config& config);
    ~Socket();

    /**
     * Opens the socket to the configured host and port. For UDP, it sets
     * destination to send datagrams, so it always succeeds.
     * For TCP, it effectively connects to the host, so if connection fails, it returns false.
     *
     * @return True if the socket was successfully opened, false otherwise.
     */
    bool open() override;

    /**
     * Closes the socket and releases all resources.
     */
    void close() override;

    /**
     * Returns true if the socket is currently opened.
     *
     * @return True if the socket is open, false otherwise.
     */
    bool isOpen() const override;

    /**
     * Receives data from the socket into packet.data. For UDP, a single datagram
     * is received and packet.peer is set to the sender. For TCP, bytes are read as a stream.
     *
     * @param packet The packet that receives the data and, for UDP, the sender endpoint.
     * @param maxSize The maximum number of bytes to receive (default: -1 for all available data).
     * @param timeoutMs The timeout in milliseconds (default: -1 for no timeout).
     * @return The number of bytes actually received.
     */
    std::size_t receive(Packet& packet, int maxSize = -1, int timeoutMs = -1) override;

    /**
     * Sends the packet data through the socket. For UDP, the destination is the one
     * configured by open().
     *
     * @param packet The packet containing the data to send.
     * @return The number of bytes actually sent.
     */
    std::size_t send(const Packet& packet) override;

protected:
    Config config_;
    // holds OS specific data
    struct SocketHandle;
    std::unique_ptr<SocketHandle> handle_;
};
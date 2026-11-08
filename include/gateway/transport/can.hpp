#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "gateway/transport/transport.hpp"

class Can : public ITransport {
public:
    struct Config {
        std::string interface;
        unsigned int bitrate = 500000;
    };

    /**
     * Constructs a closed CAN interface object.
     */
    Can();
    explicit Can(const Config& config);

    /**
     * Closes the interface if it is still open and releases the OS handle.
     */
    ~Can();

    /**
     * Opens the CAN interface using its configured settings.
     *
     * @return True if the interface was successfully opened, false otherwise.
     */
    bool open() override;

    /**
     * Closes the CAN interface and releases the underlying OS handle.
     * Calling close() on an already closed interface has no effect.
     */
    void close() override;

    /**
     * Returns true if the CAN interface is currently opened.
     *
     * @return True if the interface is open, false otherwise.
     */
    bool isOpen() const override;

    /**
     * Receives data from the CAN interface into packet.data and stores the
     * frame identifier in packet.id. The packet data is replaced with the
     * bytes actually received.
     *
     * @param packet The packet that receives the data and the frame identifier.
     * @param maxSize The maximum number of bytes to receive (default: -1 for the
     *                maximum CAN frame payload).
     * @param timeoutMs The maximum time in milliseconds to wait for a frame. If
     *                  zero or negative, waits indefinitely (default: -1).
     * @return The number of bytes actually received.
     */
    std::size_t receive(Packet& packet, int maxSize = -1, int timeoutMs = -1) override;

    /**
     * Sends the packet data as a CAN frame using packet.id as the frame identifier.
     *
     * @param packet The packet containing the data and identifier to send.
     * @return The number of bytes actually sent.
     */
    std::size_t send(const Packet& packet) override;

protected:
    Config config_;
    struct CanHandle;
    std::unique_ptr<CanHandle> handle_;
};

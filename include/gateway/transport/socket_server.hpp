#pragma once

class SocketServer {
public:
    SocketServer();
    ~SocketServer();

    enum Protocol {
        TCP = 0,
        UDP,
    };

    bool open(const std::string& port, Protocol protocol = Protocol::TCP);
    void close();
    bool isOpen() const;

    std::size_t read(std::vector<std::uint8_t>& buffer, int maxSize = -1, int timeoutMs = -1);
    std::size_t write(std::vector<std::uint8_t>& buffer);

protected:
    struct SocketHandle;
    std::unique_ptr<SocketHandle> handle_;
};
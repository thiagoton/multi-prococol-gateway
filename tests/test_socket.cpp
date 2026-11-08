#include "gateway/transport/socket.hpp"

#include "support/loopback.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kTimeoutMs = 1000;

}

TEST(SocketTest, StartsClosed)
{
    Socket socket;

    EXPECT_FALSE(socket.isOpen());
}

TEST(SocketTest, ClosedSocketDoesNotSendOrReceive)
{
    Socket socket;
    Packet packet;
    packet.data = {0x01};

    EXPECT_EQ(socket.send(packet), 0U);
    EXPECT_EQ(socket.receive(packet), 0U);
}

TEST(SocketTest, TcpOpenFailsWhenNothingListens)
{
    Socket socket(Socket::Config { "127.0.0.1", std::to_string(support::unusedTcpPort()), Socket::Protocol::TCP });

    EXPECT_FALSE(socket.open());
    EXPECT_FALSE(socket.isOpen());
}

TEST(SocketTest, UdpOpenSucceedsWithoutPeer)
{
    Socket socket(Socket::Config { "127.0.0.1", std::to_string(support::unusedTcpPort()), Socket::Protocol::UDP });

    EXPECT_TRUE(socket.open());
    EXPECT_TRUE(socket.isOpen());
}

class SocketTcpTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        listener_ = support::listenTcp(port_);
        socket_ = std::make_unique<Socket>(Socket::Config { "127.0.0.1", std::to_string(port_), Socket::Protocol::TCP });
        ASSERT_TRUE(socket_->open());
        peer_ = support::FileDescriptor(::accept(listener_.get(), nullptr, nullptr));
        ASSERT_TRUE(peer_);
    }

    std::uint16_t port_ = 0;
    support::FileDescriptor listener_;
    support::FileDescriptor peer_;
    std::unique_ptr<Socket> socket_;
};

TEST_F(SocketTcpTest, ReportsOpenAfterConnect)
{
    EXPECT_TRUE(socket_->isOpen());
}

TEST_F(SocketTcpTest, SendDeliversBytesToPeer)
{
    Packet packet;
    packet.data = {0x01, 0x02, 0x03, 0x04, 0x05};

    EXPECT_EQ(socket_->send(packet), 5U);
    EXPECT_EQ(support::readExactly(peer_.get(), 5, kTimeoutMs), packet.data);
}

TEST_F(SocketTcpTest, ReceiveReturnsBytesFromPeer)
{
    const std::vector<std::uint8_t> bytes = {0x10, 0x20, 0x30, 0x40, 0x50};
    ASSERT_TRUE(support::writeAll(peer_.get(), bytes));

    Packet packet;
    EXPECT_EQ(socket_->receive(packet, 5, kTimeoutMs), 5U);
    EXPECT_EQ(packet.data, bytes);
}

TEST_F(SocketTcpTest, BoundedReceiveReturnsPartialDataOnTimeout)
{
    const std::vector<std::uint8_t> bytes = {0xA1, 0xA2, 0xA3};
    ASSERT_TRUE(support::writeAll(peer_.get(), bytes));

    Packet packet;
    EXPECT_EQ(socket_->receive(packet, 5, 200), 3U);
    EXPECT_EQ(packet.data, bytes);
}

TEST_F(SocketTcpTest, ReceiveWithoutDataTimesOut)
{
    Packet packet;

    EXPECT_EQ(socket_->receive(packet, 4, 50), 0U);
    EXPECT_TRUE(packet.data.empty());
}

TEST_F(SocketTcpTest, AvailableReceiveWithoutDataReturnsZero)
{
    Packet packet;

    EXPECT_EQ(socket_->receive(packet), 0U);
    EXPECT_TRUE(packet.data.empty());
}

TEST_F(SocketTcpTest, ReceiveReturnsZeroAfterPeerCloses)
{
    peer_.reset();

    Packet packet;
    EXPECT_EQ(socket_->receive(packet, 4, kTimeoutMs), 0U);
}

TEST_F(SocketTcpTest, SendAfterCloseReturnsZero)
{
    socket_->close();

    Packet packet;
    packet.data = {0x01};

    EXPECT_FALSE(socket_->isOpen());
    EXPECT_EQ(socket_->send(packet), 0U);
}

TEST_F(SocketTcpTest, ReopenAfterCloseConnectsAgain)
{
    socket_->close();

    EXPECT_TRUE(socket_->open());
    EXPECT_TRUE(socket_->isOpen());
}

class SocketUdpTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        peer_ = support::bindLoopback(SOCK_DGRAM, port_);
        socket_ = std::make_unique<Socket>(Socket::Config { "127.0.0.1", std::to_string(port_), Socket::Protocol::UDP });
        ASSERT_TRUE(socket_->open());
    }

    sockaddr_in learnGatewayAddress()
    {
        Packet hello;
        hello.data = {0x00};
        EXPECT_EQ(socket_->send(hello), 1U);

        support::Datagram datagram;
        EXPECT_TRUE(support::receiveDatagram(peer_.get(), kTimeoutMs, datagram));
        return datagram.from;
    }

    std::uint16_t port_ = 0;
    support::FileDescriptor peer_;
    std::unique_ptr<Socket> socket_;
};

TEST_F(SocketUdpTest, SendDeliversDatagramToPeer)
{
    Packet packet;
    packet.data = {0xAA, 0xBB, 0xCC};

    EXPECT_EQ(socket_->send(packet), 3U);

    support::Datagram datagram;
    ASSERT_TRUE(support::receiveDatagram(peer_.get(), kTimeoutMs, datagram));
    EXPECT_EQ(datagram.bytes, packet.data);
}

TEST_F(SocketUdpTest, ReceiveReturnsDatagramAndSenderEndpoint)
{
    const sockaddr_in gateway = learnGatewayAddress();
    const std::vector<std::uint8_t> reply = {0x10, 0x20, 0x30};
    ASSERT_TRUE(support::sendDatagram(peer_.get(), gateway, reply));

    Packet packet;
    EXPECT_EQ(socket_->receive(packet, -1, kTimeoutMs), 3U);
    EXPECT_EQ(packet.data, reply);
    EXPECT_EQ(packet.peer.host, "127.0.0.1");
    EXPECT_EQ(packet.peer.port, std::to_string(port_));
}

TEST_F(SocketUdpTest, ReceiveTimesOutWithoutDatagram)
{
    Packet packet;

    EXPECT_EQ(socket_->receive(packet, -1, 50), 0U);
    EXPECT_TRUE(packet.data.empty());
}

TEST_F(SocketUdpTest, ReceiveTruncatesDatagramToMaxSize)
{
    const sockaddr_in gateway = learnGatewayAddress();
    const std::vector<std::uint8_t> datagram = {1, 2, 3, 4, 5, 6, 7, 8};
    ASSERT_TRUE(support::sendDatagram(peer_.get(), gateway, datagram));

    Packet packet;
    EXPECT_EQ(socket_->receive(packet, 4, kTimeoutMs), 4U);
    EXPECT_EQ(packet.data, (std::vector<std::uint8_t> {1, 2, 3, 4}));

    EXPECT_EQ(socket_->receive(packet, 4, 50), 0U);
}

#include "gateway/transport/socket.hpp"

#include <gtest/gtest.h>

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

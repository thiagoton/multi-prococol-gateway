#include "gateway/transport/can.hpp"

#include <gtest/gtest.h>

TEST(CanTest, StartsClosed)
{
    Can can;

    EXPECT_FALSE(can.isOpen());
}

TEST(CanTest, ClosedCanDoesNotSendOrReceive)
{
    Can can;
    Packet packet;
    packet.data = {0x01};

    EXPECT_EQ(can.send(packet), 0U);
    EXPECT_EQ(can.receive(packet), 0U);
}

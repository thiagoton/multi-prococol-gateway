#include "gateway/transport/serial.hpp"

#include <gtest/gtest.h>

TEST(SerialTest, StartsClosed)
{
    Serial serial;

    EXPECT_FALSE(serial.isOpen());
}

TEST(SerialTest, ClosedSerialDoesNotSendOrReceive)
{
    Serial serial;
    Packet packet;
    packet.data = {0x01};

    EXPECT_EQ(serial.send(packet), 0U);
    EXPECT_EQ(serial.receive(packet), 0U);
}

#include "gateway/transport/can.hpp"

#include "support/can_peer.hpp"

#include <linux/can.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kInterface = "vcan0";
constexpr int kTimeoutMs = 1000;

}

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

TEST(CanTest, OpenFailsForUnknownInterface)
{
    Can can(Can::Config { "gateway-no-such-can0" });

    EXPECT_FALSE(can.open());
    EXPECT_FALSE(can.isOpen());
}

TEST(CanTest, OpenFailsForEmptyInterfaceName)
{
    Can can;

    EXPECT_FALSE(can.open());
    EXPECT_FALSE(can.isOpen());
}

class CanVirtualBusTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        if (!support::canInterfaceExists(kInterface))
            GTEST_SKIP() << "CAN interface " << kInterface << " not found; run tests/scripts/setup_vcan.sh";

        peer_ = std::make_unique<support::CanPeer>(kInterface);
        can_ = std::make_unique<Can>(Can::Config { kInterface, 500000 });
        ASSERT_TRUE(can_->open());
    }

    std::unique_ptr<support::CanPeer> peer_;
    std::unique_ptr<Can> can_;
};

TEST_F(CanVirtualBusTest, ReportsOpenAfterOpen)
{
    EXPECT_TRUE(can_->isOpen());
}

TEST_F(CanVirtualBusTest, SendDeliversFrameToPeer)
{
    Packet packet;
    packet.id = 0x123;
    packet.data = {1, 2, 3, 4};

    EXPECT_EQ(can_->send(packet), 4U);

    struct can_frame frame;
    ASSERT_TRUE(peer_->receive(frame, kTimeoutMs));
    EXPECT_EQ(frame.can_id, 0x123U);
    EXPECT_EQ(frame.can_dlc, 4);
    EXPECT_EQ(std::vector<std::uint8_t>(frame.data, frame.data + frame.can_dlc), packet.data);
}

TEST_F(CanVirtualBusTest, ReceiveReturnsFrameFromPeer)
{
    const std::vector<std::uint8_t> bytes = {9, 8, 7};
    ASSERT_TRUE(peer_->send(0x456, bytes));

    Packet packet;
    EXPECT_EQ(can_->receive(packet, -1, kTimeoutMs), 3U);
    EXPECT_EQ(packet.id, 0x456U);
    EXPECT_EQ(packet.data, bytes);
}

TEST_F(CanVirtualBusTest, ReceiveTimesOutWithoutFrame)
{
    Packet packet;

    EXPECT_EQ(can_->receive(packet, -1, 50), 0U);
    EXPECT_TRUE(packet.data.empty());
}

TEST_F(CanVirtualBusTest, ReceiveTruncatesPayloadToMaxSize)
{
    ASSERT_TRUE(peer_->send(0x10, {1, 2, 3, 4, 5, 6, 7, 8}));

    Packet packet;
    EXPECT_EQ(can_->receive(packet, 3, kTimeoutMs), 3U);
    EXPECT_EQ(packet.data, (std::vector<std::uint8_t> {1, 2, 3}));
}

TEST_F(CanVirtualBusTest, ExtendedIdentifierRoundTrips)
{
    const std::uint32_t extendedId = CAN_EFF_FLAG | 0x1ABCDE01U;

    Packet outbound;
    outbound.id = extendedId;
    outbound.data = {0xDE, 0xAD};
    EXPECT_EQ(can_->send(outbound), 2U);

    struct can_frame frame;
    ASSERT_TRUE(peer_->receive(frame, kTimeoutMs));
    EXPECT_EQ(frame.can_id, extendedId);

    ASSERT_TRUE(peer_->send(extendedId, {0xBE, 0xEF}));
    Packet inbound;
    EXPECT_EQ(can_->receive(inbound, -1, kTimeoutMs), 2U);
    EXPECT_EQ(inbound.id, extendedId);
}

TEST_F(CanVirtualBusTest, EmptyPayloadIsNotSent)
{
    Packet packet;
    packet.id = 0x1;

    EXPECT_EQ(can_->send(packet), 0U);

    struct can_frame frame;
    EXPECT_FALSE(peer_->receive(frame, 50));
}

TEST_F(CanVirtualBusTest, SendAfterCloseReturnsZero)
{
    can_->close();

    Packet packet;
    packet.id = 0x1;
    packet.data = {0x01};

    EXPECT_FALSE(can_->isOpen());
    EXPECT_EQ(can_->send(packet), 0U);
}

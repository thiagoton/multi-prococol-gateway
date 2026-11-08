#include "gateway/transport/serial.hpp"

#include "support/fd.hpp"
#include "support/pty.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kTimeoutMs = 1000;

}

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

TEST(SerialTest, OpenFailsForMissingPort)
{
    Serial serial(Serial::Config { "/dev/gateway-missing-port" });

    EXPECT_FALSE(serial.open());
    EXPECT_FALSE(serial.isOpen());
}

TEST(SerialTest, OpenFailsForNonTtyFile)
{
    Serial serial(Serial::Config { "/dev/null" });

    EXPECT_FALSE(serial.open());
    EXPECT_FALSE(serial.isOpen());
}

TEST(SerialTest, OpenFailsForUnsupportedBaudRate)
{
    support::PtyPair pty;
    Serial serial(Serial::Config { pty.slavePath(), 12345 });

    EXPECT_FALSE(serial.open());
    EXPECT_FALSE(serial.isOpen());
}

TEST(SerialTest, OpenAcceptsEverySupportedBaudRate)
{
    support::PtyPair pty;
    const unsigned int rates[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};

    for (unsigned int rate : rates) {
        Serial serial(Serial::Config { pty.slavePath(), rate });
        EXPECT_TRUE(serial.open()) << "baud " << rate;
        serial.close();
    }
}

TEST(SerialTest, OpenAcceptsParityAndStopBitCombinations)
{
    support::PtyPair pty;
    const Serial::Parity parities[] = {Serial::Parity::NONE, Serial::Parity::ODD, Serial::Parity::EVEN};
    const Serial::StopBits stopBits[] = {Serial::StopBits::ONE, Serial::StopBits::TWO};

    for (Serial::Parity parity : parities) {
        for (Serial::StopBits stop : stopBits) {
            Serial serial(Serial::Config { pty.slavePath(), 9600, parity, stop });
            EXPECT_TRUE(serial.open()) << "parity " << parity << " stop " << stop;
            serial.close();
        }
    }
}

class SerialPtyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        serial_ = std::make_unique<Serial>(Serial::Config { pty_.slavePath() });
        ASSERT_TRUE(serial_->open());
    }

    support::PtyPair pty_;
    std::unique_ptr<Serial> serial_;
};

TEST_F(SerialPtyTest, ReportsOpenAfterOpen)
{
    EXPECT_TRUE(serial_->isOpen());
}

TEST_F(SerialPtyTest, CloseReportsClosed)
{
    serial_->close();

    EXPECT_FALSE(serial_->isOpen());
}

TEST_F(SerialPtyTest, PeerBytesAreReceived)
{
    const std::vector<std::uint8_t> bytes = {0x10, 0x20, 0x30, 0x40, 0x50};
    ASSERT_TRUE(support::writeAll(pty_.master(), bytes));

    Packet packet;
    EXPECT_EQ(serial_->receive(packet, 5, kTimeoutMs), 5U);
    EXPECT_EQ(packet.data, bytes);
}

TEST_F(SerialPtyTest, BoundedReceiveReturnsPartialDataOnTimeout)
{
    const std::vector<std::uint8_t> bytes = {0xA1, 0xA2, 0xA3};
    ASSERT_TRUE(support::writeAll(pty_.master(), bytes));

    Packet packet;
    EXPECT_EQ(serial_->receive(packet, 5, 200), 3U);
    EXPECT_EQ(packet.data, bytes);
}

TEST_F(SerialPtyTest, ReceiveWithoutDataTimesOut)
{
    Packet packet;

    EXPECT_EQ(serial_->receive(packet, 4, 50), 0U);
    EXPECT_TRUE(packet.data.empty());
}

TEST_F(SerialPtyTest, AvailableReceiveWithoutDataReturnsZero)
{
    Packet packet;

    EXPECT_EQ(serial_->receive(packet), 0U);
    EXPECT_TRUE(packet.data.empty());
}

TEST_F(SerialPtyTest, SendDeliversBytesToPeer)
{
    Packet packet;
    packet.data = {0x01, 0x02, 0x03, 0x04};

    EXPECT_EQ(serial_->send(packet), 4U);
    EXPECT_EQ(support::readExactly(pty_.master(), 4, kTimeoutMs), packet.data);
}

TEST_F(SerialPtyTest, RawBytesRoundTripUnchanged)
{
    const std::vector<std::uint8_t> bytes = {0x00, 0x0A, 0x0D, 0xFF, 0x1A, 0x03, 0x11, 0x13};

    ASSERT_TRUE(support::writeAll(pty_.master(), bytes));
    Packet inbound;
    EXPECT_EQ(serial_->receive(inbound, static_cast<int>(bytes.size()), kTimeoutMs), bytes.size());
    EXPECT_EQ(inbound.data, bytes);

    Packet outbound;
    outbound.data = bytes;
    EXPECT_EQ(serial_->send(outbound), bytes.size());
    EXPECT_EQ(support::readExactly(pty_.master(), bytes.size(), kTimeoutMs), bytes);
}

TEST_F(SerialPtyTest, ReceiveEndsWhenPeerCloses)
{
    pty_.closeMaster();

    Packet packet;
    EXPECT_EQ(serial_->receive(packet, 4, kTimeoutMs), 0U);
}

TEST_F(SerialPtyTest, SendAfterCloseReturnsZero)
{
    serial_->close();

    Packet packet;
    packet.data = {0x01};

    EXPECT_EQ(serial_->send(packet), 0U);
}

#include "hakoniwa/pdu/foxglove/foxglove_publisher.hpp"

#include <gtest/gtest.h>

using hakoniwa::pdu::foxglove::FoxglovePublisher;

TEST(FoxglovePublisherTest, StopAndCloseAreIdempotentBeforeConfigure)
{
    FoxglovePublisher publisher;
    EXPECT_FALSE(publisher.is_running());
    EXPECT_EQ(publisher.stop(), HAKO_PDU_ERR_OK);
    EXPECT_EQ(publisher.close(), HAKO_PDU_ERR_OK);
    EXPECT_FALSE(publisher.is_running());
}

TEST(FoxglovePublisherTest, PublishBeforeStartReturnsNotRunning)
{
    FoxglovePublisher publisher;
    std::byte payload[1]{std::byte{0x42}};
    EXPECT_EQ(publisher.publish({"Robot", 1}, payload), HAKO_PDU_ERR_NOT_RUNNING);
}


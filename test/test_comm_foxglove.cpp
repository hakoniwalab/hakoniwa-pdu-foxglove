#include "hakoniwa/pdu/foxglove/comm_foxglove.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using hakoniwa::pdu::PduDef;
using hakoniwa::pdu::PduDefinition;
using hakoniwa::pdu::PduRecord;
using hakoniwa::pdu::PduResolvedKey;
using hakoniwa::pdu::foxglove::FoxgloveComm;
using hakoniwa::pdu::foxglove::FoxgloveConfig;
using hakoniwa::pdu::foxglove::IFoxglovePublisher;

namespace {

class MockPublisher final : public IFoxglovePublisher {
public:
    HakoPduErrorType start_result = HAKO_PDU_ERR_OK;
    bool configured = false;
    bool running = false;
    PduResolvedKey last_key;
    std::vector<std::byte> last_payload;

    HakoPduErrorType configure(const FoxgloveConfig& config) override
    {
        configured = !config.channels.empty();
        return HAKO_PDU_ERR_OK;
    }
    HakoPduErrorType start() noexcept override
    {
        if (start_result == HAKO_PDU_ERR_OK) {
            running = true;
        }
        return start_result;
    }
    HakoPduErrorType publish(const PduResolvedKey& key, std::span<const std::byte> payload) noexcept override
    {
        if (!running) {
            return HAKO_PDU_ERR_NOT_RUNNING;
        }
        if (!(key == PduResolvedKey{"Robot", 2})) {
            return HAKO_PDU_ERR_INVALID_PDU_KEY;
        }
        last_key = key;
        last_payload.assign(payload.begin(), payload.end());
        return HAKO_PDU_ERR_OK;
    }
    HakoPduErrorType stop() noexcept override
    {
        running = false;
        return HAKO_PDU_ERR_OK;
    }
    HakoPduErrorType close() noexcept override
    {
        running = false;
        configured = false;
        return HAKO_PDU_ERR_OK;
    }
    bool is_running() const noexcept override
    {
        return running;
    }
};

fs::path make_dir()
{
    auto dir = fs::temp_directory_path() / "hako_foxglove_comm";
    fs::remove_all(dir);
    fs::create_directories(dir / "schema");
    std::ofstream(dir / "schema" / "SimTime.msg") << "uint64 time_usec\n";
    std::ofstream(dir / "config.json") << R"json({
  "protocol": "foxglove",
  "server": {"name": "test", "host": "127.0.0.1", "port": 8765},
  "channels": [
    {
      "pdu_key": {"robot": "Robot", "pdu": "sim_time"},
      "topic": "/sim_time",
      "schema": {"name": "hako_msgs/msg/SimTime", "encoding": "ros2msg", "file": "schema/SimTime.msg"}
    }
  ]
})json";
    return dir;
}

std::shared_ptr<PduDefinition> make_definition()
{
    auto def = std::make_shared<PduDefinition>();
    PduDef pdu{};
    pdu.type = "hako_msgs/SimTime";
    pdu.org_name = "sim_time";
    pdu.name = "sim_time";
    pdu.channel_id = 2;
    pdu.pdu_size = 8;
    def->add_definition("Robot", pdu);
    return def;
}

} // namespace

TEST(FoxgloveCommTest, SendBeforeStartReturnsNotRunning)
{
    const auto dir = make_dir();
    auto mock = std::make_shared<MockPublisher>();
    FoxgloveComm comm(mock);
    comm.set_pdu_definition(make_definition());
    ASSERT_EQ(comm.open((dir / "config.json").string()), HAKO_PDU_ERR_OK);

    std::byte payload[2]{std::byte{0x01}, std::byte{0x02}};
    EXPECT_EQ(comm.send({"Robot", 2}, payload), HAKO_PDU_ERR_NOT_RUNNING);
}

TEST(FoxgloveCommTest, SendForwardsExactBytes)
{
    const auto dir = make_dir();
    auto mock = std::make_shared<MockPublisher>();
    FoxgloveComm comm(mock);
    comm.set_pdu_definition(make_definition());
    ASSERT_EQ(comm.open((dir / "config.json").string()), HAKO_PDU_ERR_OK);
    ASSERT_EQ(comm.start(), HAKO_PDU_ERR_OK);

    std::vector<std::byte> payload{std::byte{0x00}, std::byte{0x01}, std::byte{0xfe}};
    EXPECT_EQ(comm.send({"Robot", 2}, payload), HAKO_PDU_ERR_OK);
    EXPECT_EQ(mock->last_key.channel_id, 2);
    EXPECT_EQ(mock->last_payload, payload);
}

TEST(FoxgloveCommTest, UnknownResolvedKey)
{
    const auto dir = make_dir();
    auto mock = std::make_shared<MockPublisher>();
    FoxgloveComm comm(mock);
    comm.set_pdu_definition(make_definition());
    ASSERT_EQ(comm.open((dir / "config.json").string()), HAKO_PDU_ERR_OK);
    ASSERT_EQ(comm.start(), HAKO_PDU_ERR_OK);

    std::byte payload[1]{std::byte{0}};
    EXPECT_EQ(comm.send({"Robot", 99}, payload), HAKO_PDU_ERR_INVALID_PDU_KEY);
}

TEST(FoxgloveCommTest, ReceiveMethodsUnsupported)
{
    FoxgloveComm comm(std::make_shared<MockPublisher>());
    std::byte buffer[8]{};
    size_t received = 42;
    PduRecord record;
    EXPECT_EQ(comm.recv({"Robot", 2}, buffer, received), HAKO_PDU_ERR_UNSUPPORTED);
    EXPECT_EQ(received, 0U);
    EXPECT_EQ(comm.recv_next(record), HAKO_PDU_ERR_UNSUPPORTED);
    EXPECT_EQ(comm.set_recv_event({"Robot", 2}), HAKO_PDU_ERR_UNSUPPORTED);
}

TEST(FoxgloveCommTest, RepeatedStopAndCloseAreSafe)
{
    const auto dir = make_dir();
    auto mock = std::make_shared<MockPublisher>();
    FoxgloveComm comm(mock);
    comm.set_pdu_definition(make_definition());
    ASSERT_EQ(comm.open((dir / "config.json").string()), HAKO_PDU_ERR_OK);
    ASSERT_EQ(comm.start(), HAKO_PDU_ERR_OK);

    EXPECT_EQ(comm.stop(), HAKO_PDU_ERR_OK);
    EXPECT_EQ(comm.stop(), HAKO_PDU_ERR_OK);
    EXPECT_EQ(comm.close(), HAKO_PDU_ERR_OK);
    EXPECT_EQ(comm.close(), HAKO_PDU_ERR_OK);
}

TEST(FoxgloveCommTest, StartFailureDoesNotEnterRunningState)
{
    const auto dir = make_dir();
    auto mock = std::make_shared<MockPublisher>();
    mock->start_result = HAKO_PDU_ERR_IO_ERROR;
    FoxgloveComm comm(mock);
    comm.set_pdu_definition(make_definition());
    ASSERT_EQ(comm.open((dir / "config.json").string()), HAKO_PDU_ERR_OK);
    EXPECT_EQ(comm.start(), HAKO_PDU_ERR_IO_ERROR);
    bool running = true;
    EXPECT_EQ(comm.is_running(running), HAKO_PDU_ERR_OK);
    EXPECT_FALSE(running);
}


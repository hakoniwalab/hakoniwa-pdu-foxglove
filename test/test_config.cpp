#include "hakoniwa/pdu/foxglove/foxglove_config.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using hakoniwa::pdu::PduDef;
using hakoniwa::pdu::PduDefinition;
using hakoniwa::pdu::foxglove::FoxgloveConfig;
using hakoniwa::pdu::foxglove::load_foxglove_config;

namespace {

fs::path make_dir(const std::string& name)
{
    auto dir = fs::temp_directory_path() / ("hako_foxglove_" + name);
    fs::remove_all(dir);
    fs::create_directories(dir / "schema");
    return dir;
}

void write_file(const fs::path& path, const std::string& body)
{
    std::ofstream ofs(path);
    ASSERT_TRUE(ofs) << path;
    ofs << body;
}

PduDefinition make_definition()
{
    PduDefinition def;
    PduDef pdu{};
    pdu.type = "hako_msgs/SimTime";
    pdu.org_name = "sim_time";
    pdu.name = "sim_time";
    pdu.channel_id = 7;
    pdu.pdu_size = 8;
    pdu.method_type = "buffer";
    def.add_definition("Robot", pdu);
    return def;
}

std::string valid_config(const std::string& schema_file = "schema/SimTime.msg")
{
    return R"json({
  "protocol": "foxglove",
  "server": {"name": "test", "host": "127.0.0.1", "port": 8765},
  "channels": [
    {
      "pdu_key": {"robot": "Robot", "pdu": "sim_time"},
      "topic": "/sim_time",
      "schema": {
        "name": "hako_msgs/msg/SimTime",
        "encoding": "ros2msg",
        "file": ")json" + schema_file + R"json("
      }
    }
  ]
})json";
}

HakoPduErrorType load_body(
    const fs::path& dir,
    const std::string& body,
    FoxgloveConfig& config,
    const PduDefinition& def)
{
    write_file(dir / "config.json", body);
    return load_foxglove_config((dir / "config.json").string(), &def, config);
}

} // namespace

TEST(FoxgloveConfigTest, ValidConfigResolvesSchemaAndPdu)
{
    const auto dir = make_dir("valid");
    write_file(dir / "schema" / "SimTime.msg", "uint64 time_usec\n");
    const auto def = make_definition();

    FoxgloveConfig config;
    EXPECT_EQ(load_body(dir, valid_config(), config, def), HAKO_PDU_ERR_OK);
    ASSERT_EQ(config.channels.size(), 1U);
    EXPECT_EQ(config.server.port, 8765);
    EXPECT_EQ(config.channels[0].resolved_key.robot, "Robot");
    EXPECT_EQ(config.channels[0].resolved_key.channel_id, 7);
    EXPECT_EQ(config.channels[0].schema.encoding, "ros2msg");
    EXPECT_FALSE(config.channels[0].schema.data.empty());
    EXPECT_TRUE(config.channels[0].schema.file.is_absolute());
}

TEST(FoxgloveConfigTest, MalformedJson)
{
    const auto dir = make_dir("malformed");
    const auto def = make_definition();
    FoxgloveConfig config;
    EXPECT_EQ(load_body(dir, "{", config, def), HAKO_PDU_ERR_INVALID_JSON);
}

TEST(FoxgloveConfigTest, InvalidProtocol)
{
    const auto dir = make_dir("bad_protocol");
    write_file(dir / "schema" / "SimTime.msg", "uint64 time_usec\n");
    const auto def = make_definition();
    FoxgloveConfig config;
    auto body = valid_config();
    body.replace(body.find("foxglove"), std::string("foxglove").size(), "tcp");
    EXPECT_EQ(load_body(dir, body, config, def), HAKO_PDU_ERR_INVALID_CONFIG);
}

TEST(FoxgloveConfigTest, InvalidPort)
{
    const auto dir = make_dir("bad_port");
    write_file(dir / "schema" / "SimTime.msg", "uint64 time_usec\n");
    const auto def = make_definition();
    FoxgloveConfig config;
    auto body = valid_config();
    body.replace(body.find("8765"), 4, "70000");
    EXPECT_EQ(load_body(dir, body, config, def), HAKO_PDU_ERR_INVALID_CONFIG);
}

TEST(FoxgloveConfigTest, MissingSchemaFile)
{
    const auto dir = make_dir("missing_schema");
    const auto def = make_definition();
    FoxgloveConfig config;
    EXPECT_EQ(load_body(dir, valid_config(), config, def), HAKO_PDU_ERR_FILE_NOT_FOUND);
}

TEST(FoxgloveConfigTest, UnsupportedSchemaEncoding)
{
    const auto dir = make_dir("bad_encoding");
    write_file(dir / "schema" / "SimTime.msg", "uint64 time_usec\n");
    const auto def = make_definition();
    FoxgloveConfig config;
    auto body = valid_config();
    body.replace(body.find("ros2msg"), std::string("ros2msg").size(), "jsonschema");
    EXPECT_EQ(load_body(dir, body, config, def), HAKO_PDU_ERR_INVALID_CONFIG);
}

TEST(FoxgloveConfigTest, UnresolvedPduName)
{
    const auto dir = make_dir("unresolved");
    write_file(dir / "schema" / "SimTime.msg", "uint64 time_usec\n");
    const PduDefinition def;
    FoxgloveConfig config;
    EXPECT_EQ(load_body(dir, valid_config(), config, def), HAKO_PDU_ERR_INVALID_PDU_KEY);
}

TEST(FoxgloveConfigTest, DuplicatePduMapping)
{
    const auto dir = make_dir("dup_pdu");
    write_file(dir / "schema" / "SimTime.msg", "uint64 time_usec\n");
    const auto def = make_definition();
    FoxgloveConfig config;
    auto body = valid_config();
    body.replace(body.rfind("]"), 1, R"json(,
    {
      "pdu_key": {"robot": "Robot", "pdu": "sim_time"},
      "topic": "/other",
      "schema": {"name": "hako_msgs/msg/SimTime", "encoding": "ros2msg", "file": "schema/SimTime.msg"}
    }
  ])json");
    EXPECT_EQ(load_body(dir, body, config, def), HAKO_PDU_ERR_INVALID_CONFIG);
}

TEST(FoxgloveConfigTest, DuplicateTopic)
{
    const auto dir = make_dir("dup_topic");
    write_file(dir / "schema" / "SimTime.msg", "uint64 time_usec\n");
    auto def = make_definition();
    PduDef other{};
    other.type = "hako_msgs/SimTime";
    other.org_name = "other";
    other.name = "other";
    other.channel_id = 8;
    other.pdu_size = 8;
    def.add_definition("Robot", other);
    FoxgloveConfig config;
    auto body = valid_config();
    body.replace(body.rfind("]"), 1, R"json(,
    {
      "pdu_key": {"robot": "Robot", "pdu": "other"},
      "topic": "/sim_time",
      "schema": {"name": "hako_msgs/msg/SimTime", "encoding": "ros2msg", "file": "schema/SimTime.msg"}
    }
  ])json");
    EXPECT_EQ(load_body(dir, body, config, def), HAKO_PDU_ERR_INVALID_CONFIG);
}

TEST(FoxgloveConfigTest, MissingPduDefinition)
{
    const auto dir = make_dir("missing_pdu_def");
    write_file(dir / "schema" / "SimTime.msg", "uint64 time_usec\n");
    FoxgloveConfig config;
    write_file(dir / "config.json", valid_config());
    EXPECT_EQ(load_foxglove_config((dir / "config.json").string(), nullptr, config),
              HAKO_PDU_ERR_INVALID_CONFIG);
}


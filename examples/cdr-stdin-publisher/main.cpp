#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/foxglove/comm_foxglove.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

namespace fs = std::filesystem;

std::atomic_bool g_stop{false};

void handle_signal(int)
{
    g_stop.store(true);
}

struct Options {
    std::string endpoint_config = "config/shadow_hand/endpoint_foxglove_jointstate.json";
    std::uint64_t max_samples = 0;
    bool multiplex = false;
};

struct ChannelDisplayConfig {
    std::string topic;
    std::string robot;
    std::string pdu;
    std::string schema;
};

struct DisplayConfig {
    std::string host = "127.0.0.1";
    int port = 8766;
    std::string topic = "/hakoniwa/ShadowHandAsset/joint_states";
    std::string robot = "ShadowHandAsset";
    std::string pdu = "joint_states";
    std::string schema = "sensor_msgs/msg/JointState";
    std::vector<ChannelDisplayConfig> channels;
};

fs::path resolve_path(const fs::path& base_dir, const std::string& value)
{
    fs::path p(value);
    if (p.is_absolute()) {
        return p.lexically_normal();
    }
    return (base_dir / p).lexically_normal();
}

DisplayConfig load_display_config(const std::string& endpoint_config)
{
    DisplayConfig display;
    std::ifstream endpoint_stream(endpoint_config);
    if (!endpoint_stream) {
        return display;
    }
    nlohmann::json endpoint_json;
    endpoint_stream >> endpoint_json;
    const auto endpoint_dir = fs::absolute(fs::path(endpoint_config)).parent_path();
    const auto comm_path = resolve_path(endpoint_dir, endpoint_json.at("comm").get<std::string>());

    std::ifstream comm_stream(comm_path);
    if (!comm_stream) {
        return display;
    }
    nlohmann::json comm_json;
    comm_stream >> comm_json;
    display.host = comm_json.at("server").at("host").get<std::string>();
    display.port = comm_json.at("server").at("port").get<int>();
    if (comm_json.contains("channels") && comm_json.at("channels").is_array()) {
        for (const auto& channel : comm_json.at("channels")) {
            ChannelDisplayConfig ch;
            ch.topic = channel.at("topic").get<std::string>();
            ch.robot = channel.at("pdu_key").at("robot").get<std::string>();
            ch.pdu = channel.at("pdu_key").at("pdu").get<std::string>();
            ch.schema = channel.at("schema").at("name").get<std::string>();
            display.channels.push_back(ch);
        }
        if (!display.channels.empty()) {
            display.topic = display.channels.at(0).topic;
            display.robot = display.channels.at(0).robot;
            display.pdu = display.channels.at(0).pdu;
            display.schema = display.channels.at(0).schema;
        }
    }
    return display;
}

void print_usage(const char* argv0)
{
    std::cerr
        << "Usage: " << argv0 << " [options]\n"
        << "  --endpoint-config PATH  Default: config/shadow_hand/endpoint_foxglove_jointstate.json\n"
        << "  --samples COUNT         Default: 0 (run until stdin EOF or Ctrl-C)\n"
        << "  --multiplex             Read frames as <u16 channel-index><u32 size><payload>\n";
}

bool parse_args(int argc, char* argv[], Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << std::endl;
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--endpoint-config") {
            const char* value = require_value("--endpoint-config");
            if (value == nullptr) return false;
            options.endpoint_config = value;
        } else if (arg == "--samples") {
            const char* value = require_value("--samples");
            if (value == nullptr) return false;
            options.max_samples = std::stoull(value);
        } else if (arg == "--multiplex") {
            options.multiplex = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            return false;
        }
    }
    return true;
}

bool read_exact(std::istream& in, char* buffer, std::size_t size)
{
    in.read(buffer, static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(in.gcount()) == size;
}

std::uint32_t decode_le_u32(const std::array<char, 4>& bytes)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3])) << 24);
}

std::uint16_t decode_le_u16(const std::array<char, 2>& bytes)
{
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[0])) |
           (static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[1])) << 8);
}

} // namespace

int main(int argc, char* argv[])
{
    Options options;
    if (!parse_args(argc, argv, options)) {
        print_usage(argv[0]);
        return 2;
    }

    std::signal(SIGINT, handle_signal);

    const auto display = load_display_config(options.endpoint_config);
    auto foxglove_comm = std::make_shared<hakoniwa::pdu::foxglove::FoxgloveComm>();
    hakoniwa::pdu::Endpoint endpoint("cdr_stdin_foxglove_publisher", HAKO_PDU_ENDPOINT_DIRECTION_OUT);
    endpoint.set_comm(foxglove_comm);

    auto err = endpoint.open(options.endpoint_config);
    if (err != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to open endpoint config: " << options.endpoint_config
                  << " err=" << static_cast<int>(err) << std::endl;
        return 1;
    }
    err = endpoint.start();
    if (err != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to start Foxglove endpoint: err=" << static_cast<int>(err) << std::endl;
        (void)endpoint.close();
        return 1;
    }

    std::cerr << "Foxglove WebSocket: ws://" << display.host << ":" << display.port << std::endl;
    std::vector<hakoniwa::pdu::PduKey> keys;
    for (std::size_t i = 0; i < display.channels.size(); ++i) {
        const auto& ch = display.channels.at(i);
        keys.push_back(hakoniwa::pdu::PduKey{ch.robot, ch.pdu});
        std::cerr << "Channel[" << i << "]: " << ch.topic
                  << " schema=" << ch.schema << " key=" << ch.robot << "/" << ch.pdu << std::endl;
    }
    if (keys.empty()) {
        keys.push_back(hakoniwa::pdu::PduKey{display.robot, display.pdu});
        std::cerr << "Publishing topic: " << display.topic << std::endl;
        std::cerr << "Schema: " << display.schema << " (ros2msg), message encoding: cdr" << std::endl;
    }
    std::cerr << "Reading "
              << (options.multiplex ? "multiplexed" : "length-prefixed")
              << " CDR frames from stdin." << std::endl;

    std::uint64_t published = 0;
    while (!g_stop.load() && (options.max_samples == 0 || published < options.max_samples)) {
        std::size_t channel_index = 0;
        if (options.multiplex) {
            std::array<char, 2> channel_header{};
            if (!read_exact(std::cin, channel_header.data(), channel_header.size())) {
                break;
            }
            channel_index = decode_le_u16(channel_header);
            if (channel_index >= keys.size()) {
                std::cerr << "Invalid multiplex channel index: " << channel_index << std::endl;
                (void)endpoint.stop();
                (void)endpoint.close();
                return 1;
            }
        }
        std::array<char, 4> header{};
        if (!read_exact(std::cin, header.data(), header.size())) {
            break;
        }
        const std::uint32_t size = decode_le_u32(header);
        if (size == 0) {
            continue;
        }
        std::vector<std::uint8_t> payload(size);
        if (!read_exact(std::cin, reinterpret_cast<char*>(payload.data()), payload.size())) {
            std::cerr << "Truncated CDR frame on stdin" << std::endl;
            (void)endpoint.stop();
            (void)endpoint.close();
            return 1;
        }
        err = endpoint.send(keys.at(channel_index), std::as_bytes(std::span{payload}));
        if (err != HAKO_PDU_ERR_OK) {
            std::cerr << "Foxglove CDR send failed: err=" << static_cast<int>(err) << std::endl;
            (void)endpoint.stop();
            (void)endpoint.close();
            return 1;
        }
        if ((published % 20) == 0) {
            std::cerr << "published CDR frame samples=" << published
                      << " channel=" << channel_index
                      << " bytes=" << payload.size() << std::endl;
        }
        ++published;
    }

    (void)endpoint.stop();
    (void)endpoint.close();
    return 0;
}

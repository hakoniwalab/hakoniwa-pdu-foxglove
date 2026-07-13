#include "hakoniwa/pdu/endpoint.hpp"
#include "hakoniwa/pdu/foxglove/comm_foxglove.hpp"

#include "hako_msgs/pdu_cpptype_cdr_conv_SimTime.hpp"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;

std::atomic_bool g_stop{false};

void handle_signal(int)
{
    g_stop.store(true);
}

std::vector<std::byte> to_byte_payload(const std::vector<std::uint8_t>& src)
{
    std::vector<std::byte> dst(src.size());
    for (std::size_t i = 0; i < src.size(); ++i) {
        dst[i] = static_cast<std::byte>(src[i]);
    }
    return dst;
}

struct DisplayConfig {
    std::string host = "127.0.0.1";
    int port = 8765;
    std::string topic = "/hakoniwa/FoxgloveDemo/sim_time";
    std::string robot = "FoxgloveDemo";
    std::string pdu = "sim_time";
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
    if (comm_json.contains("channels") && comm_json.at("channels").is_array() &&
        !comm_json.at("channels").empty()) {
        const auto& channel = comm_json.at("channels").at(0);
        display.topic = channel.at("topic").get<std::string>();
        display.robot = channel.at("pdu_key").at("robot").get<std::string>();
        display.pdu = channel.at("pdu_key").at("pdu").get<std::string>();
    }
    return display;
}

} // namespace

int main(int argc, char* argv[])
{
    const std::string endpoint_config =
        (argc > 1) ? argv[1] : "config/sample/endpoint_foxglove.json";
    const std::uint64_t max_samples = (argc > 2) ? std::stoull(argv[2]) : 0ULL;
    const auto display = load_display_config(endpoint_config);

    std::signal(SIGINT, handle_signal);

    auto foxglove_comm = std::make_shared<hakoniwa::pdu::foxglove::FoxgloveComm>();
    hakoniwa::pdu::Endpoint endpoint("foxglove_cdr_publisher", HAKO_PDU_ENDPOINT_DIRECTION_OUT);
    endpoint.set_comm(foxglove_comm);

    auto err = endpoint.open(endpoint_config);
    if (err != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to open endpoint config: " << endpoint_config
                  << " err=" << static_cast<int>(err) << std::endl;
        return 1;
    }
    err = endpoint.start();
    if (err != HAKO_PDU_ERR_OK) {
        std::cerr << "Failed to start Foxglove endpoint: err=" << static_cast<int>(err) << std::endl;
        (void)endpoint.close();
        return 1;
    }

    std::cout << "Foxglove WebSocket: ws://" << display.host << ":" << display.port << std::endl;
    std::cout << "Publishing topic: " << display.topic << std::endl;
    std::cout << "Schema: hako_msgs/msg/SimTime (ros2msg), message encoding: cdr" << std::endl;
    std::cout << "Press Ctrl-C to stop." << std::endl;

    hako::pdu::msgs::hako_msgs::SimTimeCdr converter;
    const hakoniwa::pdu::PduKey key{display.robot, display.pdu};
    std::uint64_t tick = 0;

    while (!g_stop.load() && (max_samples == 0 || tick < max_samples)) {
        HakoCpp_SimTime value{};
        value.time_usec = tick * 100000ULL;

        std::vector<std::uint8_t> cdr_payload;
        if (converter.cpp2cdr(value, cdr_payload) < 0) {
            std::cerr << "SimTime CDR conversion failed" << std::endl;
            (void)endpoint.stop();
            (void)endpoint.close();
            return 1;
        }

        const auto payload = to_byte_payload(cdr_payload);
        err = endpoint.send(key, std::span<const std::byte>(payload.data(), payload.size()));
        if (err != HAKO_PDU_ERR_OK) {
            std::cerr << "Foxglove CDR send failed: err=" << static_cast<int>(err) << std::endl;
            (void)endpoint.stop();
            (void)endpoint.close();
            return 1;
        }

        std::cout << "published time_usec=" << value.time_usec
                  << " bytes=" << payload.size() << std::endl;
        ++tick;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    (void)endpoint.stop();
    (void)endpoint.close();
    return 0;
}

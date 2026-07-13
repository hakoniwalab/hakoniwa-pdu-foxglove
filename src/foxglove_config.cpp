#include "hakoniwa/pdu/foxglove/foxglove_config.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace hakoniwa::pdu::foxglove {
namespace {

bool non_empty_string(const nlohmann::json& obj, const char* key, std::string& out)
{
    if (!obj.contains(key) || !obj.at(key).is_string()) {
        return false;
    }
    out = obj.at(key).get<std::string>();
    return !out.empty();
}

fs::path resolve_path(const fs::path& base_dir, const std::string& path)
{
    fs::path p(path);
    if (p.is_absolute()) {
        return p.lexically_normal();
    }
    return (base_dir / p).lexically_normal();
}

bool read_file_bytes(const fs::path& path, std::vector<std::byte>& out)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        return false;
    }
    std::vector<char> chars{
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>()};
    out.resize(chars.size());
    for (std::size_t i = 0; i < chars.size(); ++i) {
        out[i] = static_cast<std::byte>(static_cast<unsigned char>(chars[i]));
    }
    return true;
}

} // namespace

bool is_supported_schema_encoding(const std::string& encoding) noexcept
{
    return encoding == "omgidl" || encoding == "ros2msg" || encoding == "ros2idl";
}

HakoPduErrorType load_foxglove_config(
    const std::string& config_path,
    const PduDefinition* pdu_definition,
    FoxgloveConfig& out_config) noexcept
{
    if (config_path.empty()) {
        return HAKO_PDU_ERR_INVALID_ARGUMENT;
    }
    if (pdu_definition == nullptr) {
        std::cerr << "Foxglove config requires a PduDefinition" << std::endl;
        return HAKO_PDU_ERR_INVALID_CONFIG;
    }

    try {
        std::ifstream ifs(config_path);
        if (!ifs) {
            std::cerr << "Foxglove config file not found: " << config_path << std::endl;
            return HAKO_PDU_ERR_FILE_NOT_FOUND;
        }

        nlohmann::json root;
        ifs >> root;

        if (!root.contains("protocol") || !root.at("protocol").is_string() ||
            root.at("protocol").get<std::string>() != "foxglove") {
            std::cerr << "Foxglove config protocol must be 'foxglove'" << std::endl;
            return HAKO_PDU_ERR_INVALID_CONFIG;
        }
        if (!root.contains("server") || !root.at("server").is_object()) {
            return HAKO_PDU_ERR_INVALID_CONFIG;
        }

        FoxgloveConfig cfg;
        const auto& server = root.at("server");
        if (!non_empty_string(server, "name", cfg.server.name) ||
            !non_empty_string(server, "host", cfg.server.host) ||
            !server.contains("port") || !server.at("port").is_number_integer()) {
            return HAKO_PDU_ERR_INVALID_CONFIG;
        }
        const int port = server.at("port").get<int>();
        if (port <= 0 || port > 65535) {
            std::cerr << "Foxglove server port is out of range: " << port << std::endl;
            return HAKO_PDU_ERR_INVALID_CONFIG;
        }
        cfg.server.port = static_cast<std::uint16_t>(port);

        if (!root.contains("channels") || !root.at("channels").is_array() ||
            root.at("channels").empty()) {
            return HAKO_PDU_ERR_INVALID_CONFIG;
        }

        const fs::path base_dir = fs::absolute(fs::path(config_path)).parent_path();
        std::set<std::pair<std::string, std::string>> pdu_keys;
        std::set<std::string> topics;

        for (const auto& channel : root.at("channels")) {
            if (!channel.is_object() ||
                !channel.contains("pdu_key") || !channel.at("pdu_key").is_object() ||
                !channel.contains("schema") || !channel.at("schema").is_object()) {
                return HAKO_PDU_ERR_INVALID_CONFIG;
            }

            FoxgloveChannelConfig ch;
            const auto& key = channel.at("pdu_key");
            if (!non_empty_string(key, "robot", ch.pdu_key.robot) ||
                !non_empty_string(key, "pdu", ch.pdu_key.pdu) ||
                !non_empty_string(channel, "topic", ch.topic)) {
                return HAKO_PDU_ERR_INVALID_CONFIG;
            }
            if (!pdu_keys.emplace(ch.pdu_key.robot, ch.pdu_key.pdu).second) {
                std::cerr << "Duplicate Foxglove PDU mapping: " << ch.pdu_key.robot
                          << "/" << ch.pdu_key.pdu << std::endl;
                return HAKO_PDU_ERR_INVALID_CONFIG;
            }
            if (!topics.insert(ch.topic).second) {
                std::cerr << "Duplicate Foxglove topic: " << ch.topic << std::endl;
                return HAKO_PDU_ERR_INVALID_CONFIG;
            }

            PduDef def;
            if (!pdu_definition->resolve(ch.pdu_key.robot, ch.pdu_key.pdu, def)) {
                std::cerr << "Unresolved Foxglove PDU mapping: " << ch.pdu_key.robot
                          << "/" << ch.pdu_key.pdu << std::endl;
                return HAKO_PDU_ERR_INVALID_PDU_KEY;
            }
            ch.resolved_key = {ch.pdu_key.robot, def.channel_id};

            const auto& schema = channel.at("schema");
            std::string schema_file;
            if (!non_empty_string(schema, "name", ch.schema.name) ||
                !non_empty_string(schema, "encoding", ch.schema.encoding) ||
                !non_empty_string(schema, "file", schema_file)) {
                return HAKO_PDU_ERR_INVALID_CONFIG;
            }
            if (!is_supported_schema_encoding(ch.schema.encoding)) {
                std::cerr << "Unsupported Foxglove schema encoding: " << ch.schema.encoding << std::endl;
                return HAKO_PDU_ERR_INVALID_CONFIG;
            }
            ch.schema.file = resolve_path(base_dir, schema_file);
            if (!fs::exists(ch.schema.file) || !fs::is_regular_file(ch.schema.file)) {
                std::cerr << "Foxglove schema file not found: " << ch.schema.file << std::endl;
                return HAKO_PDU_ERR_FILE_NOT_FOUND;
            }
            if (!read_file_bytes(ch.schema.file, ch.schema.data)) {
                std::cerr << "Failed to read Foxglove schema file: " << ch.schema.file << std::endl;
                return HAKO_PDU_ERR_IO_ERROR;
            }
            cfg.channels.push_back(std::move(ch));
        }

        out_config = std::move(cfg);
        return HAKO_PDU_ERR_OK;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Invalid Foxglove JSON: " << e.what() << std::endl;
        return HAKO_PDU_ERR_INVALID_JSON;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "Invalid Foxglove config JSON shape: " << e.what() << std::endl;
        return HAKO_PDU_ERR_INVALID_CONFIG;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load Foxglove config: " << e.what() << std::endl;
        return HAKO_PDU_ERR_INVALID_CONFIG;
    }
}

} // namespace hakoniwa::pdu::foxglove

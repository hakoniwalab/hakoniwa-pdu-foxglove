#pragma once

#include "hakoniwa/pdu/endpoint_types.hpp"
#include "hakoniwa/pdu/pdu_definition.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace hakoniwa::pdu::foxglove {

inline constexpr const char* kFoxgloveMessageEncoding = "cdr";

struct FoxgloveServerConfig {
    std::string name;
    std::string host;
    std::uint16_t port = 0;
};

struct FoxgloveSchemaConfig {
    std::string name;
    std::string encoding;
    std::filesystem::path file;
    std::vector<std::byte> data;
};

struct FoxgloveChannelConfig {
    PduKey pdu_key;
    PduResolvedKey resolved_key;
    std::string topic;
    FoxgloveSchemaConfig schema;
};

struct FoxgloveConfig {
    FoxgloveServerConfig server;
    std::vector<FoxgloveChannelConfig> channels;
};

HakoPduErrorType load_foxglove_config(
    const std::string& config_path,
    const PduDefinition* pdu_definition,
    FoxgloveConfig& out_config) noexcept;

bool is_supported_schema_encoding(const std::string& encoding) noexcept;

} // namespace hakoniwa::pdu::foxglove

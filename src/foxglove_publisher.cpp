#include "hakoniwa/pdu/foxglove/foxglove_publisher.hpp"

#include <foxglove/channel.hpp>
#include <foxglove/context.hpp>
#include <foxglove/error.hpp>
#include <foxglove/schema.hpp>
#include <foxglove/websocket.hpp>

#include <chrono>
#include <iostream>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace hakoniwa::pdu::foxglove {
namespace {

std::uint64_t wall_time_ns()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

HakoPduErrorType map_foxglove_error(::foxglove::FoxgloveError err)
{
    if (err == ::foxglove::FoxgloveError::Ok) {
        return HAKO_PDU_ERR_OK;
    }
    return HAKO_PDU_ERR_IO_ERROR;
}

} // namespace

struct FoxglovePublisher::Impl {
    mutable std::mutex mutex;
    FoxgloveConfig config;
    bool configured = false;
    bool running = false;
    ::foxglove::Context context;
    std::optional<::foxglove::WebSocketServer> server;
    std::unordered_map<PduResolvedKey, ::foxglove::RawChannel, PduResolvedKeyHash> channels;
};

FoxglovePublisher::FoxglovePublisher()
    : impl_(std::make_unique<Impl>())
{
}

FoxglovePublisher::~FoxglovePublisher()
{
    (void)close();
}

HakoPduErrorType FoxglovePublisher::configure(const FoxgloveConfig& config)
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->running) {
        return HAKO_PDU_ERR_BUSY;
    }
    impl_->config = config;
    impl_->configured = true;
    return HAKO_PDU_ERR_OK;
}

HakoPduErrorType FoxglovePublisher::start() noexcept
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->running) {
        return HAKO_PDU_ERR_OK;
    }
    if (!impl_->configured) {
        return HAKO_PDU_ERR_INVALID_CONFIG;
    }

    try {
        impl_->context = ::foxglove::Context::create();

        ::foxglove::WebSocketServerOptions options;
        options.context = impl_->context;
        options.name = impl_->config.server.name;
        options.host = impl_->config.server.host;
        options.port = impl_->config.server.port;
        options.supported_encodings = {kFoxgloveMessageEncoding};

        auto server_result = ::foxglove::WebSocketServer::create(std::move(options));
        if (!server_result.has_value()) {
            std::cerr << "Failed to create Foxglove WebSocket server: "
                      << ::foxglove::strerror(server_result.error()) << std::endl;
            return map_foxglove_error(server_result.error());
        }
        impl_->server.emplace(std::move(server_result.value()));

        for (const auto& ch : impl_->config.channels) {
            ::foxglove::Schema schema;
            schema.name = ch.schema.name;
            schema.encoding = ch.schema.encoding;
            schema.data = ch.schema.data.empty() ? nullptr : ch.schema.data.data();
            schema.data_len = ch.schema.data.size();

            auto channel_result = ::foxglove::RawChannel::create(
                ch.topic,
                kFoxgloveMessageEncoding,
                schema,
                impl_->context);
            if (!channel_result.has_value()) {
                std::cerr << "Failed to create Foxglove RawChannel for " << ch.topic << ": "
                          << ::foxglove::strerror(channel_result.error()) << std::endl;
                for (auto& entry : impl_->channels) {
                    entry.second.close();
                }
                impl_->channels.clear();
                (void)impl_->server->stop();
                impl_->server.reset();
                return map_foxglove_error(channel_result.error());
            }
            impl_->channels.emplace(ch.resolved_key, std::move(channel_result.value()));
        }

        impl_->running = true;
        return HAKO_PDU_ERR_OK;
    } catch (const std::exception& e) {
        std::cerr << "Foxglove publisher startup failed: " << e.what() << std::endl;
        impl_->channels.clear();
        impl_->server.reset();
        impl_->running = false;
        return HAKO_PDU_ERR_IO_ERROR;
    }
}

HakoPduErrorType FoxglovePublisher::publish(
    const PduResolvedKey& key,
    std::span<const std::byte> cdr_payload) noexcept
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->running) {
        return HAKO_PDU_ERR_NOT_RUNNING;
    }
    auto it = impl_->channels.find(key);
    if (it == impl_->channels.end()) {
        return HAKO_PDU_ERR_INVALID_PDU_KEY;
    }
    const std::byte* data = cdr_payload.empty() ? nullptr : cdr_payload.data();
    const auto err = it->second.log(data, cdr_payload.size(), wall_time_ns());
    if (err != ::foxglove::FoxgloveError::Ok) {
        std::cerr << "Foxglove RawChannel log failed: " << ::foxglove::strerror(err) << std::endl;
    }
    return map_foxglove_error(err);
}

HakoPduErrorType FoxglovePublisher::stop() noexcept
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->running) {
        return HAKO_PDU_ERR_OK;
    }
    for (auto& entry : impl_->channels) {
        entry.second.close();
    }
    impl_->channels.clear();
    if (impl_->server.has_value()) {
        const auto err = impl_->server->stop();
        impl_->server.reset();
        impl_->running = false;
        return map_foxglove_error(err);
    }
    impl_->running = false;
    return HAKO_PDU_ERR_OK;
}

HakoPduErrorType FoxglovePublisher::close() noexcept
{
    const auto err = stop();
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->configured = false;
    impl_->config = {};
    return err;
}

bool FoxglovePublisher::is_running() const noexcept
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->running;
}

} // namespace hakoniwa::pdu::foxglove

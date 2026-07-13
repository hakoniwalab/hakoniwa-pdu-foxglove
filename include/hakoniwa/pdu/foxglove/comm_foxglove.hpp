#pragma once

#include "hakoniwa/pdu/comm/comm.hpp"
#include "hakoniwa/pdu/foxglove/foxglove_publisher.hpp"

#include <memory>
#include <mutex>

namespace hakoniwa::pdu::foxglove {

class FoxgloveComm final : public PduComm {
public:
    FoxgloveComm();
    explicit FoxgloveComm(std::shared_ptr<IFoxglovePublisher> publisher);
    ~FoxgloveComm() override = default;

    HakoPduErrorType open(const std::string& config_path) override;
    HakoPduErrorType close() noexcept override;
    HakoPduErrorType start() noexcept override;
    HakoPduErrorType stop() noexcept override;
    HakoPduErrorType is_running(bool& running) noexcept override;
    HakoPduErrorType send(const PduResolvedKey& pdu_key, std::span<const std::byte> data) noexcept override;
    HakoPduErrorType recv(
        const PduResolvedKey& pdu_key,
        std::span<std::byte> data,
        size_t& received_size) noexcept override;
    HakoPduErrorType recv_next(PduRecord& out) noexcept override;
    HakoPduErrorType set_recv_event(const PduResolvedKey& pdu_key) noexcept override;

private:
    enum class State {
        Closed,
        Open,
        Running
    };

    std::shared_ptr<IFoxglovePublisher> publisher_;
    FoxgloveConfig config_;
    State state_ = State::Closed;
    mutable std::mutex mutex_;
};

} // namespace hakoniwa::pdu::foxglove


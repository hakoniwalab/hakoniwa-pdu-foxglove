#pragma once

#include "hakoniwa/pdu/foxglove/foxglove_config.hpp"

#include <memory>
#include <span>

namespace hakoniwa::pdu::foxglove {

class IFoxglovePublisher {
public:
    virtual ~IFoxglovePublisher() = default;

    virtual HakoPduErrorType configure(const FoxgloveConfig& config) = 0;
    virtual HakoPduErrorType start() noexcept = 0;
    virtual HakoPduErrorType publish(
        const PduResolvedKey& key,
        std::span<const std::byte> cdr_payload) noexcept = 0;
    virtual HakoPduErrorType stop() noexcept = 0;
    virtual HakoPduErrorType close() noexcept = 0;
    virtual bool is_running() const noexcept = 0;
};

class FoxglovePublisher final : public IFoxglovePublisher {
public:
    FoxglovePublisher();
    ~FoxglovePublisher() override;

    FoxglovePublisher(const FoxglovePublisher&) = delete;
    FoxglovePublisher& operator=(const FoxglovePublisher&) = delete;

    HakoPduErrorType configure(const FoxgloveConfig& config) override;
    HakoPduErrorType start() noexcept override;
    HakoPduErrorType publish(
        const PduResolvedKey& key,
        std::span<const std::byte> cdr_payload) noexcept override;
    HakoPduErrorType stop() noexcept override;
    HakoPduErrorType close() noexcept override;
    bool is_running() const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hakoniwa::pdu::foxglove

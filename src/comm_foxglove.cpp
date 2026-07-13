#include "hakoniwa/pdu/foxglove/comm_foxglove.hpp"

#include <iostream>

namespace hakoniwa::pdu::foxglove {

FoxgloveComm::FoxgloveComm()
    : FoxgloveComm(std::make_shared<FoxglovePublisher>())
{
}

FoxgloveComm::FoxgloveComm(std::shared_ptr<IFoxglovePublisher> publisher)
    : publisher_(std::move(publisher))
{
}

HakoPduErrorType FoxgloveComm::open(const std::string& config_path)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!publisher_) {
        return HAKO_PDU_ERR_INVALID_CONFIG;
    }
    if (state_ == State::Running) {
        return HAKO_PDU_ERR_BUSY;
    }
    if (!pdu_def_) {
        std::cerr << "FoxgloveComm requires PduDefinition before open()" << std::endl;
        return HAKO_PDU_ERR_INVALID_CONFIG;
    }

    FoxgloveConfig parsed;
    auto err = load_foxglove_config(config_path, pdu_def_.get(), parsed);
    if (err != HAKO_PDU_ERR_OK) {
        return err;
    }
    err = publisher_->configure(parsed);
    if (err != HAKO_PDU_ERR_OK) {
        return err;
    }
    config_ = std::move(parsed);
    state_ = State::Open;
    return HAKO_PDU_ERR_OK;
}

HakoPduErrorType FoxgloveComm::close() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    HakoPduErrorType err = HAKO_PDU_ERR_OK;
    if (publisher_) {
        err = publisher_->close();
    }
    config_ = {};
    state_ = State::Closed;
    return err;
}

HakoPduErrorType FoxgloveComm::start() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::Running) {
        return HAKO_PDU_ERR_OK;
    }
    if (state_ != State::Open || !publisher_) {
        return HAKO_PDU_ERR_INVALID_CONFIG;
    }
    const auto err = publisher_->start();
    if (err != HAKO_PDU_ERR_OK) {
        return err;
    }
    state_ = State::Running;
    return HAKO_PDU_ERR_OK;
}

HakoPduErrorType FoxgloveComm::stop() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::Running) {
        return HAKO_PDU_ERR_OK;
    }
    HakoPduErrorType err = HAKO_PDU_ERR_OK;
    if (publisher_) {
        err = publisher_->stop();
    }
    state_ = State::Open;
    return err;
}

HakoPduErrorType FoxgloveComm::is_running(bool& running) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    running = (state_ == State::Running) && publisher_ && publisher_->is_running();
    return HAKO_PDU_ERR_OK;
}

HakoPduErrorType FoxgloveComm::send(
    const PduResolvedKey& pdu_key,
    std::span<const std::byte> data) noexcept
{
    std::shared_ptr<IFoxglovePublisher> publisher;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != State::Running || !publisher_) {
            return HAKO_PDU_ERR_NOT_RUNNING;
        }
        publisher = publisher_;
    }
    return publisher->publish(pdu_key, data);
}

HakoPduErrorType FoxgloveComm::recv(
    const PduResolvedKey& pdu_key,
    std::span<std::byte> data,
    size_t& received_size) noexcept
{
    (void)pdu_key;
    (void)data;
    received_size = 0;
    return HAKO_PDU_ERR_UNSUPPORTED;
}

HakoPduErrorType FoxgloveComm::recv_next(PduRecord& out) noexcept
{
    (void)out;
    return HAKO_PDU_ERR_UNSUPPORTED;
}

HakoPduErrorType FoxgloveComm::set_recv_event(const PduResolvedKey& pdu_key) noexcept
{
    (void)pdu_key;
    return HAKO_PDU_ERR_UNSUPPORTED;
}

} // namespace hakoniwa::pdu::foxglove


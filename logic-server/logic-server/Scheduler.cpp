#include "Scheduler.h"

Scheduler::Scheduler(IoContext::strand& strand, const std::chrono::milliseconds cycleTime, TaskHandler handler)
    : _strand(strand), _cycleTime(cycleTime), _handler(std::move(handler))
{
    _timer = std::make_shared<boost::asio::steady_timer>(strand.context());
}

void Scheduler::Start()
{
    auto self(shared_from_this());
    boost::asio::post(_strand, [self] { self->DoStart(); });
}

void Scheduler::DoStart()
{
    auto self(shared_from_this());
    _timer->expires_after(_cycleTime);
    _timer->async_wait([self](const boost::system::error_code& ec)
    {
        if (ec)
        {
            if (ec == boost::asio::error::operation_aborted)
            {
                spdlog::info("scheduler aborted");
                return; // Timer was stopped
            }

            spdlog::error("scheduler timer error: {}", ec.message());
            return;
        }

        self->_handler([self]() { self->Start(); });
    });
}

void Scheduler::Stop(bool forceStop)
{
    auto self(shared_from_this());
    boost::asio::post(_strand, [self] { self->_timer->cancel(); });
}
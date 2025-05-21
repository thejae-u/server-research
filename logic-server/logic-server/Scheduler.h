#pragma once
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <functional>
#include <chrono>
#include <spdlog/spdlog.h>

class Scheduler
{
public:
    using IoContext = boost::asio::io_context;
    
    explicit Scheduler(IoContext& ctx, const std::chrono::milliseconds cycleTime, std::function<void()> handler)
        : _cycleTime(cycleTime), _handler(std::move(handler))
    {
        _timer = std::make_unique<boost::asio::steady_timer>(ctx);
    }

    ~Scheduler() = default;
    void Start();
    void Stop() const;

private:
    std::unique_ptr<boost::asio::steady_timer> _timer;
    std::chrono::milliseconds _cycleTime;
    std::function<void()> _handler;
};

inline void Scheduler::Start()
{
    _timer->expires_after(_cycleTime);
    _timer->async_wait([this](const boost::system::error_code& ec)
    {
        if (ec)
        {
            if (ec == boost::asio::error::operation_aborted || ec == boost::asio::error::eof)
            {
                return; // Timer was stopped
            }

            SPDLOG_ERROR("Timer Error: {}",  ec.message());
            return;
        }

        _handler(); // Execute the task
        Start(); // Restart the timer
    });
}

inline void Scheduler::Stop() const
{
    _timer->cancel();
}

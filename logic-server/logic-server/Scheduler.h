#pragma once
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <functional>
#include <chrono>
#include <spdlog/spdlog.h>

class Scheduler : public std::enable_shared_from_this<Scheduler>
{
public:
    using IoContext = boost::asio::io_context;
    
    explicit Scheduler(IoContext::strand& strand, const std::chrono::milliseconds cycleTime, std::function<void()> handler)
        : _strand(strand), _cycleTime(cycleTime), _handler(std::move(handler))
    {
        _timer = std::make_unique<boost::asio::steady_timer>(strand.context());
    }

    ~Scheduler() = default;
    void Start();
    void Stop();

private:
    IoContext::strand& _strand;
    std::unique_ptr<boost::asio::steady_timer> _timer;
    std::chrono::milliseconds _cycleTime;
    std::function<void()> _handler;

    void DoStart();
    void DoStop();
};

inline void Scheduler::Start()
{
    auto self(shared_from_this());
    boost::asio::post(_strand.wrap([self]{ self->DoStart(); }));
}

inline void Scheduler::DoStart()
{
    auto self(shared_from_this());
    _timer->expires_after(_cycleTime);
    _timer->async_wait(_strand.wrap([self](const boost::system::error_code& ec)
    {
        if (ec)
        {
            if (ec == boost::asio::error::operation_aborted)
            {
                return; // Timer was stopped
            }

            SPDLOG_ERROR("Scheduler Timer Error: {}", ec.message());
            return;
        }

        self->_handler(); // Execute the task
        self->Start(); // Restart the timer
    }));
}

inline void Scheduler::Stop()
{
    auto self(shared_from_this());
    boost::asio::post(_strand.wrap([self]{ self->DoStop();} ));
}

inline void Scheduler::DoStop()
{
    _timer->cancel();
}

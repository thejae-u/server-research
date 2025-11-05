#pragma once
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <functional>
#include <chrono>
#include <spdlog/spdlog.h>

#include "Base.h"

using CompletionHandler = std::function<void()>;
using TaskHandler = std::function<void(CompletionHandler)>;

class Scheduler : public Base<Scheduler>
{
public:
    using IoContext = boost::asio::io_context;

    Scheduler(IoContext::strand& strand, const std::chrono::milliseconds cycleTime, TaskHandler handler);
    void Start() override;
    void Stop() override;


private:
    IoContext::strand& _strand;
    std::shared_ptr<boost::asio::steady_timer> _timer;
    std::chrono::milliseconds _cycleTime;
    TaskHandler _handler;

    void DoStart();
};

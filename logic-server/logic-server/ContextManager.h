#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

class ContextManager : public std::enable_shared_from_this<ContextManager>
{
public:
    explicit ContextManager(const std::size_t threadCount)
        : _workStrand(_ctx), _workGuard(boost::asio::make_work_guard(_ctx))
    {
        _ctxThreads.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i)
        {
            _ctxThreads.emplace_back(std::make_shared<std::thread>([this]() { _ctx.run(); }));
        }
    }

    ~ContextManager()
    {
        for (const auto& ctxThread : _ctxThreads)
        {
            if (ctxThread->joinable())
            {
                ctxThread->join();
            }
        }
        
        SPDLOG_INFO("ContextManager destroyed and all threads joined.");
    }

    void Stop();

    boost::asio::io_context& GetContext(){ return _ctx; }
    boost::asio::io_context::strand& GetStrand() { return _workStrand; }
    
private:
    boost::asio::io_context _ctx;
    boost::asio::io_context::strand _workStrand;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _workGuard;
    std::vector<std::shared_ptr<std::thread>> _ctxThreads;
};

inline void ContextManager::Stop()
{
    _workGuard.reset(); // Stop the context from running
    _ctx.stop(); // Stop the io_context
    SPDLOG_INFO("ContextManager stopped.");
}


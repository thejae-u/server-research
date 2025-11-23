#pragma once
#include <memory>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>

using ThreadPool = boost::asio::thread_pool;
class ContextManager : public std::enable_shared_from_this<ContextManager>
{
private:
    struct PrivateInternalTag {};

public:
    explicit ContextManager(PrivateInternalTag, std::string contextName, const std::size_t blockingThreadCount);
    static std::shared_ptr<ContextManager> Create(std::string contextName, const std::size_t threadCount, const std::size_t blockingThreadCount = 4 /*Default 4 thread are blocking thread*/);
    ~ContextManager();
    void Stop();

    // For Use io_context normally
    boost::asio::io_context& GetContext() { return _ctx; }

    // For Use BlockingPool (use this for heavy work)
    ThreadPool& GetBlockingPool() { return _blockingPool; }

private:
    boost::asio::io_context _ctx;
    ThreadPool _blockingPool;

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _workGuard;
    std::vector<std::shared_ptr<std::thread>> _ctxThreads;

    std::string_view _contextName;
};

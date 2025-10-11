#pragma once
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <memory>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

using ThreadPool = boost::asio::thread_pool;
class ContextManager : public std::enable_shared_from_this<ContextManager>
{
public:
	explicit ContextManager(const std::size_t threadCount, const std::size_t blockingThreadCount = 4 /*Default 4 thread are blocking thread*/)
		: _workStrand(_ctx), _blockingPool(blockingThreadCount), _workGuard(boost::asio::make_work_guard(_ctx))
	{
		_ctxThreads.reserve(threadCount);
		for (std::size_t i = 0; i < threadCount; ++i)
		{
			_ctxThreads.emplace_back(std::make_shared<std::thread>([this]() { _ctx.run(); }));
		}
	}

	~ContextManager()
	{
		// All Thread safe End Task
		for (const auto& ctxThread : _ctxThreads)
		{
			if (ctxThread->joinable())
			{
				ctxThread->join();
			}
		}

		// also blocking thread
		_blockingPool.join();

		spdlog::info("context manager destroyed and all threads joined.");
	}

	void Stop();

	// For Use io_context normally
	boost::asio::io_context& GetContext() { return _ctx; }

	// For Use Strand (Avoid data race)
	boost::asio::io_context::strand& GetStrand() { return _workStrand; }

	// For Use BlockingPool (use this for heavy work)
	ThreadPool& GetBlockingPool() { return _blockingPool; }

private:
	boost::asio::io_context _ctx;
	boost::asio::io_context::strand _workStrand;
	ThreadPool _blockingPool;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _workGuard;
	std::vector<std::shared_ptr<std::thread>> _ctxThreads;
};

inline void ContextManager::Stop()
{
	_workGuard.reset(); // Stop the context from running
	_ctx.stop(); // Stop the io_context
	_blockingPool.stop(); // Stop blocking pool
	spdlog::info("context manager stopped.");
}

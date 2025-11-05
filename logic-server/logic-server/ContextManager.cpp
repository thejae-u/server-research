#include "ContextManager.h"

ContextManager::ContextManager(PrivateInternalTag, std::string contextName, const std::size_t blockingThreadCount)
    : _contextName(contextName), _workStrand(_ctx), _blockingPool(blockingThreadCount), _workGuard(boost::asio::make_work_guard(_ctx))
{
}

std::shared_ptr<ContextManager> ContextManager::Create(std::string contextName, const std::size_t threadCount, const std::size_t blockingThreadCount)
{
    auto manager = std::make_shared<ContextManager>(PrivateInternalTag{}, contextName, blockingThreadCount);
    manager->_ctxThreads.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i)
    {
        manager->_ctxThreads.emplace_back(std::make_shared<std::thread>([manager]() { manager->_ctx.run(); }));
    }

    return manager;
}

ContextManager::~ContextManager()
{
	// All Thread safe End Task
	for (const auto& ctxThread : _ctxThreads)
	{
		if (ctxThread->joinable())
		{
			ctxThread->join();
		}

		_ctx.stop();
	}
	spdlog::info("{} context manager thread join complete", _contextName);

	// also blocking threa
	_blockingPool.join();
	spdlog::info("{} context manager blocking pool join complete", _contextName);
	spdlog::info("{} context manager destroyed and all threads joined.", _contextName);
}

void ContextManager::Stop()
{
	_workGuard.reset(); // Stop the context from running
	_ctx.stop(); // Stop the io_context
	_blockingPool.stop(); // Stop blocking pool
	spdlog::info("context manager stopped.");
}
#include "ContextManager.h"

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
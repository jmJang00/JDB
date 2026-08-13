#pragma once

#include <thread>
#include <vector>
#include <string>
#include <functional>
#include <future>
#include <memory>
#include <atomic>
#include <JCore/JWindows.h>
#include <JCore/CBucketPool.h>
#include <JCore/CLockFreeQueue.h>
#include <JCore/IWorkerObserver.h>
#include <JDB/CDBConnector.h>
#include <JDB/IDBTask.h>

class CDBQueue;

class IDBWorker
{
public:
	virtual bool PostStatus(uintptr_t compKey) = 0;
};

class CDBAgent : public IDBWorker
{
public:
	CDBAgent(const FDBConfig& config);
	~CDBAgent();

	bool Initialize(int numChannel, int numThread, int queueSize, const std::vector<IWorkerObserver*>& observers);
	void Release();

public:
	int GetUseSize(int channel);
	bool PostStatus(uintptr_t compKey) override;
	void PushTask(int channel, IDBTask* task);
	void PushTaskSync(int channel, IDBTask* task);

private:
	struct SyncTaskWrapper : public IDBTask
	{
		IDBTask* _origin;
		std::shared_ptr<std::promise<void>> _promise;

		SyncTaskWrapper(IDBTask* task, std::shared_ptr<std::promise<void>> p)
			: _origin(task), _promise(p)
		{
		}

		void Destroy() override
		{
			this->~SyncTaskWrapper();
		}

		void Execute(CDBConnector* _conn) override
		{
			if (_origin)
			{
				_origin->Execute(_conn);
				FreeTask(_origin);
			}
			_promise->set_value();
		}
	};

	void DBWorkerThread(CDBConnector* connector);

private:
	HANDLE _readIocp = NULL;
	std::vector<CThread*> _threads;
	std::vector<CDBConnector*> _connectors;
	std::vector<CDBQueue*> _dbQueues;
	std::vector<IWorkerObserver*> _observers;
	int32_t _numChannel = 0;

	FDBConfig _dbConfig;
	bool _isRunning = false;
};

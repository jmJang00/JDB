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
#include <JDB/CDBConnector.h>

struct IDBTask
{
	virtual ~IDBTask() = default;

	virtual void Execute(CDBConnector* _conn) = 0;
};

class CDBAgent
{
public:
	CDBAgent();
	~CDBAgent();

	bool Initialize(int readThreadCount, const FDBConfig& config);
	void Release();

public:
	void PushReadTask(IDBTask* task);
	void PushWriteTask(IDBTask* task);
	void RequestWriteSync(IDBTask* task);
	void RequestReadSync(IDBTask* task);

	template <typename T, typename... Args>
	static T* CreateTask(Args&&... args)
	{
		void* memory = gBucketPool.get()->Alloc(sizeof(T));
		if (!memory)
		{
			return nullptr;
		}
		return new (memory) T(std::forward<Args>(args)...);
	}

	static void FreeTask(IDBTask* task)
	{
		if (!task) return;
		task->~IDBTask();
		gBucketPool.get()->Free(task);
	}

private:
	struct SyncTaskWrapper : public IDBTask
	{
		IDBTask* _origin;
		std::shared_ptr<std::promise<void>> _promise;

		SyncTaskWrapper(IDBTask* task, std::shared_ptr<std::promise<void>> p)
			: _origin(task), _promise(p)
		{
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

	void ReadWorkerThread();
	void WriteWorkerThread();

private:
	HANDLE _readIocp = NULL;
	HANDLE _writeShutdownEvent = NULL;
	HANDLE _writeEvent = NULL;

	CLockFreeQueue<IDBTask*> _writeQueue;
	std::vector<CThread*> _readThreads;
	CThread* _writeThread = nullptr;

	FDBConfig _dbConfig;
	bool _isRunning = false;
};

#pragma once
#include <stdint.h>
#include <functional>
#include <utility>
#include <future>
#include <JCore/CLockFreeQueue.h>
#include <JCore/JWindows.h>

struct IDBTask;
class IDBWorker;
class CDBConnector;

class CDBQueue
{
public:
	CDBQueue(int32_t maxTaskCnt, IDBWorker* worker);

	~CDBQueue();

	int GetUseSize();

	bool PostTask(IDBTask* task);

	void ClearPendingTasks();

	void Execute(CDBConnector* connector);

private:
	IDBWorker* _context;
	CLockFreeQueue<IDBTask*> _buffer;
	int8_t _processing;
};

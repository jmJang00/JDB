#include "pch.h"
#include <JDB/CDBQueue.h>
#include <JDB/CDBAgent.h>
#include <JDB/CDBConnector.h>
#include <JCore/SLog.h>
#include "LogTag.h"

CDBQueue::CDBQueue(int32_t maxTaskCnt, IDBWorker* worker)
	: _buffer(maxTaskCnt)
	, _processing(0)
	, _context(worker)
{
}

CDBQueue::~CDBQueue()
{
	ClearPendingTasks();
}

int CDBQueue::GetUseSize()
{
	return _buffer.GetSize();
}

bool CDBQueue::PostTask(IDBTask* task)
{
	_buffer.ForceEnqueue(task);

	if (InterlockedExchange8((char*)&_processing, 1) == 0)
	{
		_context->PostStatus((uintptr_t)this);
	}

	return true;
}

void CDBQueue::ClearPendingTasks()
{
	IDBTask* task;
	while (_buffer.GetSize() > 0)
	{
		if (!_buffer.Dequeue(&task))
		{
			break;
		}

		IDBTask::FreeTask(task);
	}
}

void CDBQueue::Execute(CDBConnector* connector)
{
	while (1)
	{
		IDBTask* task = nullptr;
		_buffer.Dequeue(&task);
		if (task == nullptr)
		{
			break;
		}

		task->Execute(connector);
		IDBTask::FreeTask(task);
	}

	InterlockedExchange8((char*)&_processing, 0);

	if (_buffer.GetSize() > 0)
	{
		if (InterlockedExchange8((char*)&_processing, 1) == 0)
		{
			_context->PostStatus((uintptr_t)this);
		}
	}
}

#include "pch.h"
#include <JDB/CDBAgent.h>
#include <JDB/CDBQueue.h>
#include "LogTag.h"

CDBAgent::CDBAgent(const FDBConfig& config)
{
	_dbConfig = config;
}

CDBAgent::~CDBAgent()
{ 
	Release(); 
}

bool CDBAgent::Initialize(int numChannel, int numThread, int queueSize, const std::vector<IWorkerObserver*>& observers)
{
	if (_isRunning)
	{
		return false;
	}

	_isRunning = true;
	_numChannel = numChannel;
	_observers = observers;
	for (int i = 0; i < observers.size(); ++i)
	{
		_observers.push_back(observers[i]->Clone());
	}

	_readIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (!_readIocp)
	{
		return false;
	}

	for (int i = 0; i < numChannel; ++i)
	{
		_dbQueues.push_back(new CDBQueue(queueSize, this));
	}

	for (int i = 0; i < numThread; ++i)
	{
		auto connector = new CDBConnector(_dbConfig);
		_connectors.push_back(connector);
		_threads.push_back(new CThread([this, connector]() { DBWorkerThread(connector); }));
		_threads.back()->Create();
	}

	return true;
}

void CDBAgent::Release()
{
	if (!_isRunning)
	{
		return;
	}

	_isRunning = false;

	for (size_t i = 0; i < _threads.size(); ++i)
	{
		PostQueuedCompletionStatus(_readIocp, 0, 0, nullptr);
	}

	for (int i = 0; i < _connectors.size(); i++)
	{
		_connectors[i]->Disable();
	}

	for (auto& t : _threads)
	{
		t->Wait();
		t->Close();
	}

	if (_readIocp) 
	{ 
		CloseHandle(_readIocp); 
		_readIocp = nullptr; 
	}

	for (auto& t : _threads)
	{
		if (t != nullptr)
		{
			delete t;
			t = nullptr;
		}
	}
	_threads.clear();

	for (auto& q : _dbQueues)
	{
		if (q != nullptr)
		{
			delete q;
			q = nullptr;
		}
	}
	_dbQueues.clear();

	for (int i = 0; i < _connectors.size(); i++)
	{
		delete _connectors[i];
	}

	_connectors.clear();

	for (int i = 0; i < _observers.size(); i++)
	{
		delete _observers[i];
	}
	_observers.clear();
}

int CDBAgent::GetUseSize(int channel)
{
	if (channel < 0 || channel >= _numChannel)
		return 0;

	_dbQueues[channel]->GetUseSize();
}

bool CDBAgent::PostStatus(uintptr_t compKey)
{
	return PostQueuedCompletionStatus(_readIocp, 0, (ULONG_PTR)compKey, nullptr);
}

void CDBAgent::PushTask(int channel, IDBTask* task)
{
	if (channel < 0 || channel >= _numChannel)
		return;

	_dbQueues[channel]->PostTask(task);
}

void CDBAgent::PushTaskSync(int channel, IDBTask* task)
{
	auto promise = std::make_shared<std::promise<void>>();
	auto future = promise->get_future();

	SyncTaskWrapper wrapper(task, promise);

	PushTask(channel, &wrapper);

	future.get();
}

void CDBAgent::DBWorkerThread(CDBConnector* connector)
{
	connector->Enable();
	connector->Reconnect();
	for (auto observer : _observers)
	{
		observer->OnWorkerEnter();
	}

	while (_isRunning)
	{
		DWORD bytesTransferred = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED overlapped = nullptr;

		for (auto observer : _observers)
		{
			observer->OnWorkerEnd();
		}
		GetQueuedCompletionStatus(_readIocp, &bytesTransferred, &completionKey, &overlapped, INFINITE);

		IDBTask* task = reinterpret_cast<IDBTask*>(completionKey);
		if (task == nullptr && completionKey == 0 && overlapped == nullptr)
		{
			break;
		}

		CDBQueue* queue = (CDBQueue*)completionKey;
		queue->Execute(connector);
	}

	connector->Disconnect();
	for (auto observer : _observers)
	{
		observer->OnWorkerExit();
	}
}

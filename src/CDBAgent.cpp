#include "pch.h"
#include <JDB/CDBAgent.h>
#include "LogTag.h"

CDBAgent::CDBAgent()
	: _writeQueue(10000)
{

}

CDBAgent::~CDBAgent()
{ 
	Release(); 
}

bool CDBAgent::Initialize(int readThreadCount, const FDBConfig& config)
{
	if (_isRunning)
	{
		return false;
	}

	_dbConfig = config;
	_isRunning = true;

	_readIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (!_readIocp)
	{
		return false;
	}

	_writeEvent = CreateEvent(nullptr, false, false, nullptr);
	_writeShutdownEvent = CreateEvent(nullptr, false, false, nullptr);

	for (int i = 0; i < readThreadCount; ++i)
	{
		_readThreads.push_back(new CThread([this]() { this->ReadWorkerThread(); }));
		_readThreads.back()->Create();
	}

	_writeThread = new CThread([this]() { this->WriteWorkerThread(); });
	_writeThread->Create();

	return true;
}

void CDBAgent::Release()
{
	if (!_isRunning)
	{
		return;
	}

	_isRunning = false;

	for (size_t i = 0; i < _readThreads.size(); ++i)
	{
		PostQueuedCompletionStatus(_readIocp, 0, 0, nullptr);
	}

	SetEvent(_writeShutdownEvent);

	for (auto& t : _readThreads)
	{
		t->Wait();
		t->Close();
	}

	_writeThread->Wait();
	_writeThread->Close();

	if (_readIocp) 
	{ 
		CloseHandle(_readIocp); 
		_readIocp = nullptr; 
	}

	if (_writeEvent) 
	{ 
		CloseHandle(_writeEvent); 
		_writeEvent = nullptr; 
	}

	if (_writeShutdownEvent) 
	{ 
		CloseHandle(_writeShutdownEvent); 
		_writeShutdownEvent = nullptr; 
	}

	for (auto& t : _readThreads)
	{
		if (t != nullptr)
		{
			delete t;
			t = nullptr;
		}
	}

	if (_writeThread != nullptr)
	{
		delete _writeThread;
		_writeThread = nullptr;
	}

	_readThreads.clear();
}

void CDBAgent::PushReadTask(IDBTask* task)
{
	PostQueuedCompletionStatus(_readIocp, 0, (ULONG_PTR)task, nullptr);
}

void CDBAgent::PushWriteTask(IDBTask* task)
{
	_writeQueue.Enqueue(task);
	SetEvent(_writeEvent);
}

void CDBAgent::RequestWriteSync(IDBTask* task)
{
	auto promise = std::make_shared<std::promise<void>>();
	auto future = promise->get_future();

	PushWriteTask(CreateTask<SyncTaskWrapper>(task, promise));

	future.get();
}

void CDBAgent::RequestReadSync(IDBTask* task)
{
	auto promise = std::make_shared<std::promise<void>>();
	auto future = promise->get_future();

	PushReadTask(CreateTask<SyncTaskWrapper>(task, promise));

	future.get();
}

void CDBAgent::ReadWorkerThread()
{
	CDBConnector _conn(_dbConfig);

	if (!_conn.Connect())
	{
		return;
	}

	while (_isRunning)
	{
		DWORD bytesTransferred = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED overlapped = nullptr;

		GetQueuedCompletionStatus(_readIocp, &bytesTransferred, &completionKey, &overlapped, INFINITE);

		IDBTask* task = reinterpret_cast<IDBTask*>(completionKey);
		if (task == nullptr && completionKey == 0 && overlapped == nullptr)
		{
			break;
		}

		task->Execute(&_conn);

		FreeTask(task);
	}

	_conn.Disconnect();
}

void CDBAgent::WriteWorkerThread()
{
	CDBConnector _conn(_dbConfig);
	while (!_conn.Connect())
	{
		ELOG(JDBLog::DB, L"DB Connect Fail %S:%d", _dbConfig.host.c_str(), _dbConfig.port);
		Sleep(1000);
	}

	HANDLE handles[] = { _writeShutdownEvent, _writeEvent };

	IDBTask* task = nullptr;
	while (_isRunning)
	{
		DWORD ret = WaitForMultipleObjects(2, handles, false, INFINITE);
		if (ret == WAIT_OBJECT_0)
		{
			break;
		}

		while (_writeQueue.Dequeue(&task))
		{
			task->Execute(&_conn);
			FreeTask(task);
		}
	}

	_conn.Disconnect();
}

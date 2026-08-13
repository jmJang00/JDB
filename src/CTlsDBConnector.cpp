#include "pch.h"
#include <JDB/CTlsDBConnector.h>

CTlsDBConnector::CTlsDBConnector(const FDBConfig& config)
	: _config(config)
{
	_index = TlsAlloc();
	if (_index == TLS_OUT_OF_INDEXES)
	{
		throw std::runtime_error("CTlsDBConnector::CTlsDBConnector(): tls out of indexes");
	}
}

CTlsDBConnector::~CTlsDBConnector()
{ 
	TlsFree(_index);
}

void CTlsDBConnector::Enable()
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Enable(); 
}

void CTlsDBConnector::DisableAll()
{ 
	CRGuard guard(&_lock);

	for (auto iter : _container)
	{
		iter->Disable();
	}
}

EDBError CTlsDBConnector::GetLastError()
{ 
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->GetLastError(); 
}

bool CTlsDBConnector::Connect()
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Connect();
}

void CTlsDBConnector::Reconnect()
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	connector->Reconnect();
}

void CTlsDBConnector::Disconnect()
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	connector->Disconnect();
}

bool CTlsDBConnector::BeginTransaction()
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->BeginTransaction();
}

bool CTlsDBConnector::Commit()
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Commit();
}

bool CTlsDBConnector::Rollback()
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Rollback();
}

CResultSet CTlsDBConnector::ReadQuery(const wchar_t* format, ...)
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	va_list args;

	va_start(args, format);
	CResultSet ret = connector->vReadQuery(format, args);
	va_end(args);

	return ret;
}

long long CTlsDBConnector::WriteQuery(const wchar_t* format, ...)
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	va_list args;

	va_start(args, format);
	long long ret = connector->vWriteQuery(format, args);
	va_end(args);

	return ret;
}

bool CTlsDBConnector::IsConnected()
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->IsConnected();
}

void CTlsDBConnector::PushTaskSync(IDBTask* task)
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return task->Execute(connector);
}


CDBConnector* CTlsDBConnector::Init()
{
	class CDBDeleter : public IThreadObserver
	{
	public:
		CDBDeleter(CTlsDBConnector* parent, CDBConnector* ptr, unsigned int index)
			: _parent(parent)
			, _ptr(ptr)
			, _index(index)
		{
		}

		void OnThreadExit() override
		{
			{
				CWGuard guard(&_parent->_lock);
				_parent->_container.erase(_ptr);
			}
			delete _ptr;
			TlsSetValue(_index, nullptr);
		}

		CTlsDBConnector* _parent; 
		CDBConnector* _ptr; 
		unsigned int _index;
	};

	CDBConnector* connector = new CDBConnector(_config);
	{
		CWGuard guard(&_lock);
		_container.insert(connector);
	}
	CThread::GetContextPtr()->AddObserver(Context::MakeObserver<CDeleteObserver<CDBConnector>>(connector, _index));
	TlsSetValue(_index, connector);
	return connector;
}

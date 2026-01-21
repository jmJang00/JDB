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

std::string CTlsDBConnector::Escape(const wchar_t* wvalue)
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Escape(wvalue);
}

std::string CTlsDBConnector::Escape(const std::wstring& wvalue)
{
	CDBConnector* connector = (CDBConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Escape(wvalue.c_str());
}

CDBConnector* CTlsDBConnector::Init()
{
	CDBConnector* connector = new CDBConnector(_config);
	CThread::GetContextPtr()->AddObserver(Context::MakeObserver<CDeleteObserver<CDBConnector>>(connector, _index));
	TlsSetValue(_index, connector);
	return connector;
}

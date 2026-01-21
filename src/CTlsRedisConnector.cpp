#include "pch.h"
#include <stdexcept>
#include <JDB/CTlsRedisConnector.h>

CTlsRedisConnector::CTlsRedisConnector(const FRedisConfig& config, int timeoutSec)
	: _config(config)
{
	_index = TlsAlloc();
	if (_index == TLS_OUT_OF_INDEXES)
	{
		throw std::runtime_error("CTlsRedisConnector::CTlsRedisConnector(): tls out of indexes");
	}
	_timeoutSec = timeoutSec;
}

CTlsRedisConnector::~CTlsRedisConnector()
{
	TlsFree(_index);
}

bool CTlsRedisConnector::Connect()
{
	CRedisConnector* connector = (CRedisConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Connect();
}

void CTlsRedisConnector::Disconnect()
{
	CRedisConnector* connector = (CRedisConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	connector->Disconnect();
}

bool CTlsRedisConnector::IsConnected()
{
	CRedisConnector* connector = (CRedisConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->IsConnected();
}

bool CTlsRedisConnector::Set(const std::string& key, const std::string& value)
{
	CRedisConnector* connector = (CRedisConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Set(key, value);
}

std::string CTlsRedisConnector::Get(const std::string& key)
{
	CRedisConnector* connector = (CRedisConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Get(key);
}

ERedisError CTlsRedisConnector::GetLastError()
{
	CRedisConnector* connector = (CRedisConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->GetLastError();
}

CRedisConnector* CTlsRedisConnector::Init()
{
	CRedisConnector* connector = new CRedisConnector(_config, _timeoutSec);
	CThread::GetContextPtr()->AddObserver(Context::MakeObserver<CDeleteObserver<CRedisConnector>>(connector, _index));
	TlsSetValue(_index, connector);
	return connector;
}

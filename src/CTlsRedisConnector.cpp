#include "pch.h"
#include <stdexcept>
#include <JDB/CTlsRedisConnector.h>
#include <JCore/CThread.h>

CTlsRedisConnector::CTlsRedisConnector(const FRedisConfig& config)
	: _config(config)
{
	_index = TlsAlloc();
	if (_index == TLS_OUT_OF_INDEXES)
	{
		throw std::runtime_error("CTlsRedisConnector::CTlsRedisConnector(): tls out of indexes");
	}
}

CTlsRedisConnector::~CTlsRedisConnector()
{
	TlsFree(_index);
}

void CTlsRedisConnector::Enable()
{
	CRedisConnector* connector = (CRedisConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	connector->Enable();
}

void CTlsRedisConnector::DisableAll()
{
	CRGuard guard(&_lock);

	for (auto iter : _container)
	{
		iter->Disable();
	}
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

void CTlsRedisConnector::Reconnect()
{
	CRedisConnector* connector = (CRedisConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	connector->Reconnect();
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

bool CTlsRedisConnector::Delete(const std::string& key)
{
	CRedisConnector* connector = (CRedisConnector*)TlsGetValue(_index);
	if (connector == nullptr)
	{
		connector = Init();
	}

	return connector->Delete(key);
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
	class CRedisDeleter : public IThreadObserver
	{
	public:
		CRedisDeleter(CTlsRedisConnector* parent, CRedisConnector* ptr, unsigned int index)
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

		CTlsRedisConnector* _parent; 
		CRedisConnector* _ptr; 
		unsigned int _index;
	};

	CRedisConnector* connector = new CRedisConnector(_config);
	{
		CWGuard guard(&_lock);
		_container.insert(connector);
	}
	CThread::GetContextPtr()->AddObserver(Context::MakeObserver<CRedisDeleter>(this, connector, _index));
	TlsSetValue(_index, connector);
	return connector;
}

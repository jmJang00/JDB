#pragma once
#include <set>
#include <JCore/JWindows.h>
#include <JCore/ScopedLock.h>
#include <JCore/CThread.h>
#include "CRedisConnector.h"

class CTlsRedisConnector
{
public:
	CTlsRedisConnector(const FRedisConfig& config);

	~CTlsRedisConnector();

	void Enable();

	void DisableAll();

	bool Connect();

	void Reconnect();

	void Disconnect();

	bool IsConnected();

	bool Set(const std::string& key, const std::string& value);

	std::string Get(const std::string& key);

	bool Delete(const std::string& key);

	ERedisError GetLastError();

private:
	CRedisConnector* Init();

	CRWLock _lock;
	std::set<CRedisConnector*> _container;
	unsigned int _index;
	FRedisConfig _config;
};

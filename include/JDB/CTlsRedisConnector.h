#pragma once
#include <JCore/JWindows.h>
#include <JCore/CThread.h>
#include "CRedisConnector.h"

class CTlsRedisConnector
{
public:
	CTlsRedisConnector(const FRedisConfig& config, int timeoutSec = 5);

	~CTlsRedisConnector();

	bool Connect();

	void Disconnect();

	bool IsConnected();

	bool Set(const std::string& key, const std::string& value);

	std::string Get(const std::string& key);

	ERedisError GetLastError();

private:

	CRedisConnector* Init();

	unsigned int _index;
	FRedisConfig _config;
	int _timeoutSec;
};

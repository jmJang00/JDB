#pragma once
#include <JCore/JWindows.h>
#pragma warning(push, 0)
#include <hiredis/hiredis.h>
#pragma warning(pop)
#pragma comment(lib, "hiredis.lib")
#include <string>
#include <memory>
#include <vector>

struct FRedisReplyDeleter
{
	void operator()(redisReply* r) const
	{
		if (r) freeReplyObject(r);
	}
};

using ReplyPtr = std::unique_ptr<redisReply, FRedisReplyDeleter>;

struct FRedisConfig
{
	std::string host;
	int port;
};

enum class ERedisError
{
	NONE = 0,
	CONNECTION_FAILED,
	GET_REQ_FAILED,
	SET_REQ_FAILED,
};

class CRedisConnector
{
public:
	CRedisConnector(const FRedisConfig& config, int timeoutSec = 5);
	~CRedisConnector();

	bool Connect();
	void Disconnect();
	bool IsConnected();
	ERedisError GetLastError();

	bool Set(const std::string& key, const std::string& value);
	std::string Get(const std::string& key);

private:
	ReplyPtr Execute(const char* format, ...);

	ERedisError _lastError;
	redisContext* _context;
	FRedisConfig _config;
	struct timeval _timeout;
};

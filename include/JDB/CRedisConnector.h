#pragma once
#include <JCore/JWindows.h>
#include <string>
#include <memory>
#include <vector>

struct redisReply;
struct redisContext;

struct FRedisReplyDeleter
{
	void operator()(redisReply* r) const;
};

using ReplyPtr = std::unique_ptr<redisReply, FRedisReplyDeleter>;

struct FRedisConfig
{
	std::string host;
	int port;
	int timeout;
};

enum class ERedisError
{
	NONE = 0,
	CONNECTION_FAILED,
	GET_REQ_FAILED,
	SET_REQ_FAILED,
	DELETE_REQ_FAILED,
};

class CRedisConnector
{
public:
	CRedisConnector(const FRedisConfig& config);
	~CRedisConnector();

	void Enable();
	void Disable();
	bool Connect();
	void Reconnect();
	void Disconnect();
	bool IsConnected();
	ERedisError GetLastError();

	bool Set(const std::string& key, const std::string& value, int expire = -1);
	std::string Get(const std::string& key);
	bool Delete(const std::string& key);

private:
	ReplyPtr Execute(const char* format, ...);

	bool _isRunning;
	ERedisError _lastError;
	redisContext* _context;
	FRedisConfig _config;
	struct timeval _timeout;
};

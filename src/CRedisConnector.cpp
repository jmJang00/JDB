#include "pch.h"
#include <iostream>
#include <cstring>
#include <cstdarg>
#include <JCore/SLog.h>
#include <JDB/CRedisConnector.h>
#include "LogTag.h"
#pragma warning(push, 0)
#include <hiredis/hiredis.h>
#pragma warning(pop)
#pragma comment(lib, "hiredis.lib")

void FRedisReplyDeleter::operator()(redisReply* r) const
{
	if (r) freeReplyObject(r);
}

CRedisConnector::CRedisConnector(const FRedisConfig& config)
	: _config(config)
	, _context(nullptr)
	, _lastError(ERedisError::NONE)
	, _isRunning(false)
{
	_timeout = { config.timeout, 0 };
}

CRedisConnector::~CRedisConnector()
{
	Disconnect();
}

void CRedisConnector::Enable()
{
	_isRunning = true;
}

void CRedisConnector::Disable()
{
	_isRunning = false;
}

bool CRedisConnector::Connect()
{
	if (_context != nullptr)
	{
		redisFree(_context);
		_context = nullptr;
	}

	_lastError = ERedisError::NONE;

	_context = redisConnect(_config.host.c_str(), _config.port);

	if (_context == nullptr || _context->err != 0)
	{
		if (_context)
		{
			ELOG(JDBLog::DB, L"[Redis] Connection Error: %S", _context->errstr);
			redisFree(_context);
			_context = nullptr;
		}
		else
		{
			ELOG(JDBLog::DB, L"[Redis] Connection Error: Can't allocate redis context");
		}

		_lastError = ERedisError::CONNECTION_FAILED;

		return false;
	}

	redisSetTimeout(_context, _timeout);

	return true;
}

void CRedisConnector::Reconnect()
{
	int waitTime = 500;
	while (_isRunning && !Connect())
	{
		Sleep(waitTime);

		if (waitTime < 8000)
		{
			waitTime *= 2;
		}
	}
}

void CRedisConnector::Disconnect()
{
	if (_context != nullptr)
	{
		redisFree(_context);
		_context = nullptr;
	}
}

bool CRedisConnector::IsConnected()
{
	if (_context == nullptr)
	{
		return false;
	}

	redisReply* reply = (redisReply*)redisCommand(_context, "PING");

	if (reply == nullptr)
	{
		ELOG(JDBLog::DB, L"Ping Error: %S", _context->errstr);
	}
	else
	{
		if (strcmp(reply->str, "PONG") == 0)
		{
			SLOG(JDBLog::DB, L"Connection alive!\n");
		}
		freeReplyObject(reply);
	}

	return _context != nullptr && _context->err == 0;
}

ReplyPtr CRedisConnector::Execute(const char* format, ...)
{
	if (_context == nullptr)
	{
		Reconnect();
		if (!_isRunning)
			return nullptr;
	}

	va_list ap;
	redisReply* reply = nullptr;
	while (1)
	{
		va_start(ap, format);
		reply = (redisReply*)redisvCommand(_context, format, ap);
		va_end(ap);

		if (reply == nullptr && _context->err != REDIS_OK)
		{
			ELOG(JDBLog::DB, L"[Redis] Error: %S", _context->errstr);
			Reconnect();
			if (!_isRunning)
				return nullptr;
		}
		else
		{
			return ReplyPtr(static_cast<redisReply*>(reply));
		}
	}
}

ERedisError CRedisConnector::GetLastError()
{
	return _lastError;
}

bool CRedisConnector::Set(const std::string& key, const std::string& value, int expire)
{
	ReplyPtr replyPtr;
	if (expire < 0)
	{
		 replyPtr = Execute("SET %s %s", key.c_str(), value.c_str());
	}
	else
	{
		 replyPtr = Execute("SET %s %s EX %d", key.c_str(), value.c_str(), expire);
	}

	if (replyPtr && replyPtr->type == REDIS_REPLY_STATUS && strcmp(replyPtr->str, "OK") == 0)
	{
		return true;
	}

	_lastError = ERedisError::SET_REQ_FAILED;
	return false;
}

std::string CRedisConnector::Get(const std::string& key)
{
	ReplyPtr replyPtr = Execute("GET %s", key.c_str());

	if (replyPtr && replyPtr->type == REDIS_REPLY_STRING)
	{
		return std::string(replyPtr->str, replyPtr->len);
	}

	_lastError = ERedisError::GET_REQ_FAILED;
	return "";
}

bool CRedisConnector::Delete(const std::string& key)
{
	ReplyPtr replyPtr = Execute("DELETE %s", key.c_str());

	if (replyPtr && replyPtr->type == REDIS_REPLY_INTEGER)
	{
		return true;
	}

	_lastError = ERedisError::DELETE_REQ_FAILED;
	return false;
}

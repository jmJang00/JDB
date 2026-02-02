#include "pch.h"
#include <iostream>
#include <cstring>
#include <cstdarg>
#include <JCore/SLog.h>
#include <JDB/CRedisConnector.h>
#include "LogTag.h"

CRedisConnector::CRedisConnector(const FRedisConfig& config, int timeoutSec)
	: _config(config)
	, _context(nullptr)
	, _lastError(ERedisError::NONE)
{
	_timeout = { timeoutSec, 0 };
}

CRedisConnector::~CRedisConnector()
{
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

void CRedisConnector::Disconnect()
{
	if (_context != nullptr)
	{
		redisFree(_context);
	}
}

bool CRedisConnector::IsConnected()
{
	return _context != nullptr && _context->err == 0;
}

ReplyPtr CRedisConnector::Execute(const char* format, ...)
{
	if (!IsConnected())
	{
		if (!Connect())
		{
			return nullptr;
		}
	}

	va_list ap;
	va_start(ap, format);
	redisReply* reply = (redisReply*)redisvCommand(_context, format, ap);
	va_end(ap);

	if (reply == nullptr || _context->err != REDIS_OK)
	{
		ELOG(JDBLog::DB, L"[Redis] Error: %S", _context->errstr);
		Disconnect();
		return nullptr;
	}

	return ReplyPtr(static_cast<redisReply*>(reply));
}

ERedisError CRedisConnector::GetLastError()
{
	return _lastError;
}

bool CRedisConnector::Set(const std::string& key, const std::string& value)
{
	ReplyPtr replyPtr = Execute("SET %s %s EX 20", key.c_str(), value.c_str());

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

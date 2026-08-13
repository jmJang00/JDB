#include "pch.h"
#include <JCore/SLog.h>
#include <JDB/CDBConnector.h>
#include "LogTag.h"
#include <mysql/mysql.h>
#pragma comment(lib, "libmysql.lib")


CResultSet::CResultSet(MYSQL_RES* res)
{
	if (!res)
	{
		isValid = false;
		return;
	}

	isValid = true;

	int numFields = mysql_num_fields(res);
	MYSQL_FIELD* fields = mysql_fetch_fields(res);

	for (int i = 0; i < numFields; i++)
	{
		columnIdxMap[fields[i].name] = i;
	}

	MYSQL_ROW row;
	while ((row = mysql_fetch_row(res)))
	{
		std::vector<std::string> rowData;
		for (int i = 0; i < numFields; i++)
		{
			rowData.push_back(row[i] ? row[i] : "");
		}
		data.push_back(std::move(rowData));
	}
	mysql_free_result(res);
	Next();
}

CResultSet::CResultSet(CResultSet&& other) noexcept
	: columnIdxMap(std::move(other.columnIdxMap))
	, data(std::move(other.data))
	, currentRow(other.currentRow)
{
}

CResultSet& CResultSet::operator=(CResultSet&& other) noexcept
{
	if (this != &other)
	{
		columnIdxMap = std::move(other.columnIdxMap);
		data = std::move(other.data);
		currentRow = other.currentRow;
	}
	return *this;
}

void CDBConnector::Initialize()
{
	mysql_library_init(0, nullptr, nullptr);
}

void CDBConnector::Release()
{
	mysql_library_end();
}

CDBConnector::CDBConnector(const FDBConfig& config)
	: _conn(nullptr) 
{
	mysql_thread_init();
	_config = config;
}

CDBConnector::~CDBConnector()
{ 
	if (_conn && _inTransaction)
	{
		Rollback();
	}
	Disconnect(); 
	mysql_thread_end();
}

void CDBConnector::Enable()
{
	_isRunning = true;
}

void CDBConnector::Disable()
{
	_isRunning = false;
}

int CDBConnector::GetMySqlError() const
{
	return mysql_errno(_conn);
}

bool CDBConnector::Connect()
{
	_inTransaction = false;

	if (_conn)
	{
		mysql_close(_conn);
	}

	_conn = mysql_init(NULL);

	if (!mysql_real_connect(_conn, _config.host.c_str(), _config.user.c_str(), 
			_config.pw.c_str(), _config.db.c_str(), _config.port, NULL, 0))
	{
		_lastError = EDBError::CONNECTION_FAILED;
		HandleMySQLError(mysql_errno(_conn));
		return false;
	}

	mysql_set_character_set(_conn, "utf8mb4");

	_lastError = EDBError::NONE;

	return true;
}

void CDBConnector::Reconnect()
{
	int waitTime = 50;
	while (_isRunning && !Connect())
	{
		Sleep(waitTime);

		if (waitTime < 8000)
		{
			waitTime *= 2;
		}
	}
}

void CDBConnector::Disconnect()
{
	if (_conn)
	{
		mysql_close(_conn);
		_conn = nullptr;
	}
}

bool CDBConnector::BeginTransaction()
{
	if (_inTransaction)
	{
		_lastError = EDBError::TRANSACTION_ALREADY_STARTED;
		return false;
	}

	if (Execute("START TRANSACTION"))
	{
		_inTransaction = true;
		return true;
	}

	return false;
}

bool CDBConnector::Commit()
{
	if (!_inTransaction)
	{
		_lastError = EDBError::TRANSACTION_NOT_STARTED;
		return false;
	}

	if (Execute("COMMIT"))
	{
		_inTransaction = false;
		return true;
	}

	return false;
}

bool CDBConnector::Rollback()
{
	if (!_inTransaction)
	{
		_lastError = EDBError::TRANSACTION_NOT_STARTED;
		return false;
	}

	if (Execute("ROLLBACK"))
	{
		_inTransaction = false;
		return true;
	}

	return false;
}

CResultSet CDBConnector::vReadQuery(const wchar_t* format, va_list args)
{
	if (!KeepAlive())
	{
		return CResultSet(nullptr);
	}

	char queryBuffer[MAX_QUERY_LEN];
	bool success = FormatStringW(queryBuffer, MAX_QUERY_LEN, format, args);

	if (!success)
	{
		return CResultSet(nullptr);
	}

	if (mysql_query(_conn, queryBuffer) != 0)
	{
		HandleMySQLError(mysql_errno(_conn));
		return CResultSet(nullptr);
	}

	return CResultSet(mysql_store_result(_conn));
}

CResultSet CDBConnector::ReadQuery(const wchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	CResultSet ret = vReadQuery(format, args);
	va_end(args);

	return ret;
}

long long CDBConnector::vWriteQuery(const wchar_t* format, va_list args)
{
	if (!KeepAlive())
	{
		return -1;
	}

	char queryBuffer[MAX_QUERY_LEN];
	bool success = FormatStringW(queryBuffer, MAX_QUERY_LEN, format, args);

	if (!success)
	{
		return -1;
	}

	if (mysql_query(_conn, queryBuffer) != 0)
	{
		HandleMySQLError(mysql_errno(_conn));
		return -1;
	}

	return (long long)mysql_affected_rows(_conn);
}

long long CDBConnector::WriteQuery(const wchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	long long ret = vWriteQuery(format, args);
	va_end(args);

	return ret;
}

bool CDBConnector::IsConnected()
{
	if (_conn)
	{
		return KeepAlive();
	}
	else
	{
		return false;
	}
}

std::string CDBConnector::Escape(std::wstring_view wvalue)
{
	if (!_conn || wvalue.empty())
	{
		return "";
	}
	
	std::string utf8str = StringUtil::WStringToString(wvalue);
	if (utf8str.empty())
	{
		return "";
	}

	_escapeBuffer.resize(utf8str.size() * 2 + 1);
	unsigned long len = mysql_real_escape_string(_conn, _escapeBuffer.data(), utf8str.data(), utf8str.size());

	return std::string(_escapeBuffer.data(), len);
}

bool CDBConnector::KeepAlive()
{
	if (_conn == nullptr)
	{
		Reconnect();
		
		if (!_isRunning)
			return false;
	}

	if (mysql_ping(_conn) != 0)
	{
		if (_inTransaction)
		{
			_lastError = EDBError::DISCONNECTED_DURING_TRANSACTION;
			return false;
		}

		Reconnect();

		if (!_isRunning)
			return false;

		SLOG(JDBLog::DB, L"Connection restored");
	}

	return true;
}

bool CDBConnector::Execute(const char* sql)
{
	if (!KeepAlive())
	{
		return false;
	}

	if (mysql_query(_conn, sql) != 0)
	{
		HandleMySQLError(mysql_errno(_conn));
		return false;
	}

	return true;
}

bool CDBConnector::FormatStringW(char* dest, size_t destLen, const wchar_t* format, va_list args)
{
	wchar_t wLocalBuffer[MAX_QUERY_LEN];

	va_list argsCopy;
	va_copy(argsCopy, args);
	int size = _vsnwprintf_s(wLocalBuffer, MAX_QUERY_LEN, format, argsCopy);
	va_end(argsCopy);

	if (size < 0 || (size_t)size >= MAX_QUERY_LEN)
	{
		_lastError = EDBError::QUERY_BUFFER_OVERFLOW;
		return false;
	}

	if (StringUtil::WStringToUTF8(wLocalBuffer, dest, (int)destLen) == 0)
	{
		_lastError = EDBError::UTF8_ENCODING_FAILED;
		return false;
	}

	return true;
}

void CDBConnector::HandleMySQLError(int errCode)
{
	if (errCode == 2006 || errCode == 2013)
	{
		_lastError = EDBError::CONNECTION_LOST;
		ELOG(JDBLog::DB, L"Network link failure detected: %d", errCode);
	}
	else if (errCode == 1146)
	{
		_lastError = EDBError::TABLE_NOT_FOUND;
		ELOG(JDBLog::DB, L"Table not found: %S", mysql_error(_conn));
	}
	else if (errCode == 1064)
	{
		_lastError = EDBError::SYNTAX_ERROR;
		ELOG(JDBLog::DB, L"SQL Syntax Error: %S", mysql_error(_conn));
	}
	else if (errCode == 1062)
	{
		_lastError = EDBError::DUPLICATE_KEY;
		ELOG(JDBLog::DB, L"SQL Insert Error: %S", mysql_error(_conn));
	}
	else
	{
		_lastError = EDBError::QUERY_FAILED;
		ELOG(JDBLog::DB, L"MySQL Error (%d): %S", errCode, mysql_error(_conn));
	}
}



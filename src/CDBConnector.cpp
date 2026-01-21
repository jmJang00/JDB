#include "pch.h"
#include <JCore/SLog.h>
#include <JDB/CDBConnector.h>
#include "LogTag.h"

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
		data.push_back(rowData);
	}
	mysql_free_result(res);
}

CResultSet::CResultSet(CResultSet&& other) noexcept
	: columnIdxMap(std::move(other.columnIdxMap))
	, data(std::move(other.data))
	, currentRow(other.currentRow)
{
	other.currentRow = -1;
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

CDBConnector::CDBConnector(const FDBConfig& config)
	: _conn(nullptr) 
{
	mysql_thread_init();
	_config = config;
}

CDBConnector::~CDBConnector()
{ 
	if (_conn && inTransaction)
	{
		Rollback();
	}
	Disconnect(); 
	mysql_thread_end();
}

bool CDBConnector::Connect()
{
	inTransaction = false;

	return RawConnect();
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
	if (inTransaction)
	{
		lastError = EDBError::TRANSACTION_ALREADY_STARTED;
		return false;
	}

	if (ExecuteRaw("START TRANSACTION"))
	{
		inTransaction = true;
		return true;
	}

	return false;
}

bool CDBConnector::Commit()
{
	if (!inTransaction)
	{
		lastError = EDBError::TRANSACTION_NOT_STARTED;
		return false;
	}

	if (ExecuteRaw("COMMIT"))
	{
		inTransaction = false;
		return true;
	}

	return false;
}

bool CDBConnector::Rollback()
{
	if (!inTransaction)
	{
		lastError = EDBError::TRANSACTION_NOT_STARTED;
		return false;
	}

	if (ExecuteRaw("ROLLBACK"))
	{
		inTransaction = false;
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

std::string CDBConnector::Escape(const wchar_t* wvalue)
{
	if (!_conn || !wvalue)
	{
		return "";
	}
	
	char utf8Raw[MAX_QUERY_LEN];
	int utf8Len = WStringToUTF8(wvalue, utf8Raw, sizeof(utf8Raw));
	if (utf8Len == 0)
	{
		return "";
	}

	std::vector<char> escapeBuffer(utf8Len * 2 + 1);
	mysql_real_escape_string(_conn, escapeBuffer.data(), utf8Raw, utf8Len);

	return std::string(escapeBuffer.data());
}

std::string CDBConnector::Escape(const std::wstring& wvalue)
{
	return Escape(wvalue.c_str());
}

int CDBConnector::GetMySQLError()
{
	if (_conn)
	{
		return mysql_errno(_conn);
	}
	else
	{
		return 0;
	}
}

bool CDBConnector::KeepAlive()
{
	if (mysql_ping(_conn) != 0)
	{
		if (inTransaction)
		{
			lastError = EDBError::DISCONNECTED_DURING_TRANSACTION;
			return false;
		}

		int cnt = 0;
		while (!RawConnect())
		{
			if (cnt == _config.reconnectCnt)
			{
				lastError = EDBError::QUERY_FAILED;
				return false;
			}

			cnt++;
		}

		SLOG(JDBLog::DB, L"Connection restored");
	}

	return true;
}

bool CDBConnector::RawConnect()
{
	if (_conn)
	{
		mysql_close(_conn);
	}

	_conn = mysql_init(NULL);

	if (!mysql_real_connect(_conn, _config.host.c_str(), _config.user.c_str(), 
			_config.pw.c_str(), _config.db.c_str(), _config.port, NULL, 0))
	{
		lastError = EDBError::CONNECTION_FAILED;
		return false;
	}

	mysql_set_character_set(_conn, "utf8mb4");

	lastError = EDBError::NONE;

	return true;
}

bool CDBConnector::ExecuteRaw(const char* sql)
{
	if (!KeepAlive())
	{
		return false;
	}

	if (mysql_query(_conn, sql) != 0)
	{
		lastError = EDBError::QUERY_FAILED;
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
		lastError = EDBError::QUERY_BUFFER_OVERFLOW;
		return false;
	}

	if (WStringToUTF8(wLocalBuffer, dest, (int)destLen) == 0)
	{
		return false;
	}

	return true;
}

int CDBConnector::WStringToUTF8(const wchar_t* wstr, char* dest, int destLen)
{
	if (!wstr || !dest || destLen <= 0) 
		return 0;
	
	int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, dest, destLen, NULL, NULL);

	if (utf8Size == 0)
	{
		DWORD error = ::GetLastError();
		if (error == ERROR_INSUFFICIENT_BUFFER)
		{
			lastError = EDBError::QUERY_BUFFER_OVERFLOW;
		}
		else
		{
			lastError = EDBError::UTF8_ENCODING_FAILED;
		}
		return 0;
	}

	return utf8Size;
}

void CDBConnector::HandleMySQLError(int errCode)
{
	if (errCode == 2006 || errCode == 2013)
	{
		lastError = EDBError::CONNECTION_LOST;
		SLOG(JDBLog::DB, L"Network link failure detected: %d", errCode);
	}
	else if (errCode == 1146)
	{
		lastError = EDBError::TABLE_NOT_FOUND;
		SLOG(JDBLog::DB, L"Table not found: %S", mysql_error(_conn));
	}
	else if (errCode == 1064)
	{
		lastError = EDBError::SYNTAX_ERROR;
		SLOG(JDBLog::DB, L"SQL Syntax Error: %S", mysql_error(_conn));
	}
	else
	{
		lastError = EDBError::QUERY_FAILED;
		SLOG(JDBLog::DB, L"MySQL Error (%d): %S", errCode, mysql_error(_conn));
	}
}



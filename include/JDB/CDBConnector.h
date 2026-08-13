#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <JCore/JWindows.h>
#include <JCore/StringUtility.h>

struct MYSQL_RES;
struct MYSQL;

struct FDBConfig
{
	std::string host;
	std::string user;
	std::string pw;
	std::string db;
	int port = 3306;
};

enum class EDBError
{
	NONE = 0,
	CONNECTION_FAILED,
	CONNECTION_LOST,
	SYNTAX_ERROR,
	DUPLICATE_KEY,
	QUERY_FAILED,
	UTF8_ENCODING_FAILED,
	TRANSACTION_ALREADY_STARTED,
	TRANSACTION_NOT_STARTED,
	DISCONNECTED_DURING_TRANSACTION,
	QUERY_BUFFER_OVERFLOW,
	TABLE_NOT_FOUND
};

class CResultSet
{
	std::map<std::string, int, std::less<>> columnIdxMap;
	std::vector<std::vector<std::string>> data;
	int currentRow = -1;
	bool isValid = false;

public:
	CResultSet(MYSQL_RES* res);

	CResultSet(CResultSet&& other) noexcept;

	CResultSet& operator=(CResultSet&& other) noexcept;

    // 복사 방지
    CResultSet(const CResultSet&) = delete;
    CResultSet& operator=(const CResultSet&) = delete;

	bool Next() 
	{ 
		if (currentRow < (int)data.size())
		{
			currentRow++;
			return true;
		}

		return false;
	}

	int GetRows() { return (int)data.size(); };

	bool IsValid() { return isValid; }

	bool IsEmpty() { return data.empty(); }

public:
	int GetInt(std::string_view name) 
	{ 
		auto it = columnIdxMap.find(name);
		if (it == columnIdxMap.end() || currentRow < 0 || currentRow >= (int)data.size()) 
			return 0;

		const std::string& valStr = data[currentRow][it->second];
		return atoi(valStr.c_str()); 
	}

	double GetDouble(std::string_view name) 
	{ 
		auto it = columnIdxMap.find(name);
		if (it == columnIdxMap.end() || currentRow < 0 || currentRow >= (int)data.size()) 
			return 0;

		const std::string& valStr = data[currentRow][it->second];
		return atof(valStr.c_str()); 
	}

	std::wstring GetString(std::string_view name) 
	{ 
		auto it = columnIdxMap.find(name);
		if (it == columnIdxMap.end() || currentRow < 0 || currentRow >= (int)data.size()) 
			return L"";

		return StringUtil::StringToWString(data[currentRow][it->second]);
	}
};


class CDBConnector
{
public:
	static const size_t MAX_QUERY_LEN = 4096;

public:
	static void Initialize();
	static void Release();

	CDBConnector(const FDBConfig& config);
	~CDBConnector();

	void Enable();
	void Disable();

	EDBError GetLastError() const { return _lastError; }
	int GetMySqlError() const;
	bool Connect();
	void Reconnect();
	void Disconnect();

	// --- 트랜잭션 관리 ---
	bool BeginTransaction();
	bool Commit();
	bool Rollback();

	// --- 쿼리 및 실행 ---
	CResultSet vReadQuery(const wchar_t* format, va_list args);
	CResultSet ReadQuery(const wchar_t* format, ...);
	long long vWriteQuery(const wchar_t* format, va_list args);
	long long WriteQuery(const wchar_t* format, ...);

	bool IsConnected();
	std::string Escape(std::wstring_view wvalue);

private:
	bool KeepAlive();
	bool Execute(const char* sql);
	bool FormatStringW(char* dest, size_t destLen, const wchar_t* format, va_list args);
	void HandleMySQLError(int errCode);

private:
	MYSQL* _conn;
	EDBError _lastError = EDBError::NONE;
	bool _isRunning = false;
	bool _inTransaction = false;
	std::vector<char> _escapeBuffer;
	FDBConfig _config;

};

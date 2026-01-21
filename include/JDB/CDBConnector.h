#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <JCore/JWindows.h>
#include <mysql/mysql.h>
#pragma comment(lib, "libmysql.lib")
#include <JCore/SLog.h>

struct FDBConfig
{
	std::string host;
	std::string user;
	std::string pw;
	std::string db;
	int port = 3306;
	int reconnectCnt = 3;
};

enum class EDBError
{
	NONE = 0,
	CONNECTION_FAILED,
	CONNECTION_LOST,
	SYNTAX_ERROR,
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
	std::map<std::string, int> columnIdxMap;
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

	bool Next() { return ++currentRow < (int)data.size(); }

	bool IsValid() { return isValid; }

	bool IsEmpty() { return data.empty(); }

private:
	template <typename T>
	T GetValue(const std::string& name)
	{
		auto it = columnIdxMap.find(name);
		if (it == columnIdxMap.end() || currentRow < 0 || currentRow >= (int)data.size()) 
			return T();

		const std::string& valStr = data[currentRow][it->second];

		std::stringstream ss(valStr);
		T val; 
		if (!(ss >> val))
		{
			return T();
		}

		return val;
	}

	// string 특화
	template <>
	inline std::string GetValue<std::string>(const std::string& name)
	{
		if (columnIdxMap.find(name) == columnIdxMap.end()) return "";
		return data[currentRow][columnIdxMap[name]];
	}

public:
	// 타입별 추출 함수
	int GetInt(const std::string& name) { return GetValue<int>(name); }
	double GetDouble(const std::string& name) { return GetValue<double>(name); }
	std::string GetString(const std::string& name) { return GetValue<std::string>(name); }
};


class CDBConnector
{
	MYSQL* _conn;
	EDBError lastError = EDBError::NONE;
	bool inTransaction = false;
	FDBConfig _config;

	static const size_t MAX_QUERY_LEN = 4096;

public:
	CDBConnector(const FDBConfig& config);
	~CDBConnector();

	EDBError GetLastError() const { return lastError; }
	bool Connect();
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
	std::string Escape(const wchar_t* wvalue);
	std::string Escape(const std::wstring& wvalue);

	int GetMySQLError();

private:
	bool KeepAlive();
	bool RawConnect();
	bool ExecuteRaw(const char* sql);

	bool FormatStringW(char* dest, size_t destLen, const wchar_t* format, va_list args);
	int WStringToUTF8(const wchar_t* wstr, char* dest, int destLen);

	void HandleMySQLError(int errCode);
};

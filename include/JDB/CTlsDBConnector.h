#pragma once
#include <JCore/CThread.h>
#include <JDB/CDBConnector.h>

class CTlsDBConnector
{
private:
	unsigned int _index;
	FDBConfig _config;

public:
	CTlsDBConnector(const FDBConfig& config);
	~CTlsDBConnector();

	EDBError GetLastError();
	bool Connect();
	void Disconnect();

	// --- 飘罚黎记 包府 ---
	bool BeginTransaction();
	bool Commit();

	bool Rollback();

	// --- 孽府 棺 角青 ---
	CResultSet ReadQuery(const wchar_t* format, ...);
	long long WriteQuery(const wchar_t* format, ...);
	bool IsConnected();

	std::string Escape(const wchar_t* wvalue);
	std::string Escape(const std::wstring& wvalue);

private:

	CDBConnector* Init();
};

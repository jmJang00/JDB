#pragma once
#include <set>
#include <JCore/CThread.h>
#include <JCore/ScopedLock.h>
#include <JDB/CDBConnector.h>
#include <JDB/IDBTask.h>

class CTlsDBConnector
{
public:
	CTlsDBConnector(const FDBConfig& config);
	~CTlsDBConnector();

	void Enable();
	void DisableAll();

	EDBError GetLastError();
	bool Connect();
	void Reconnect();
	void Disconnect();

	// --- 飘罚黎记 包府 ---
	bool BeginTransaction();
	bool Commit();
	bool Rollback();

	// --- 孽府 棺 角青 ---
	CResultSet ReadQuery(const wchar_t* format, ...);
	long long WriteQuery(const wchar_t* format, ...);
	bool IsConnected();

	void PushTaskSync(IDBTask* task);

private:
	CDBConnector* Init();

private:
	std::set<CDBConnector*> _container;
	CRWLock _lock;
	unsigned int _index;
	FDBConfig _config;
};

#include "pch.h"
#include <JDB/Initialize.h>
#include <JDB/CDBConnector.h>
#include <JCore/Warnings.h>
#include <JCore/Initialize.h>

long JDBInit::_initLock;
long JDBInit::_init;

DISABLE_WARNINGS_BEGIN(WARNING_28112)

void JDBInit::Initialize()
{
	if (_init == 0)
	{
		while (InterlockedExchange(&_initLock, 1) != 0)
		{
			Sleep(0);
		}

		if (_init == 0)
		{
			JCoreInit::Initialize();
			mysql_library_init(0, nullptr, nullptr);

			InterlockedExchange(&_init, 1);
		}

		InterlockedExchange(&_initLock, 0);
	}
}

void JDBInit::Release()
{
	if (_init == 1)
	{
		while (InterlockedExchange(&_initLock, 1) != 0)
		{
			Sleep(0);
		}

		if (_init == 1)
		{
			mysql_library_end();

			InterlockedExchange(&_init, 0);
		}

		InterlockedExchange(&_initLock, 0);
	}
}

DISABLE_WARNINGS_END()

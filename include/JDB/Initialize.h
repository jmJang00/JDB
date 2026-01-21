#pragma once

class JDBInit
{
public:
	static void Initialize();
	static void Release();

private:
	static long _initLock;
	static long _init;
};

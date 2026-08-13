#pragma once
#include <JCore/CBucketPool.h>

class CDBConnector;

struct IDBTask
{
	bool shouldDestroy = false;

	virtual ~IDBTask() = default;

	virtual void Execute(CDBConnector* _conn) = 0;
	virtual void Destroy() = 0;

	template <typename T, typename... Args>
	static T* CreateTask(Args&&... args)
	{
		void* memory = gBucketPool.get()->Alloc(sizeof(T));
		if (!memory)
		{
			return nullptr;
		}
		((IDBTask*)memory)->shouldDestroy = true;
		return new (memory) T(std::forward<Args>(args)...);
	}

	static void FreeTask(IDBTask* task)
	{
		if (!task) return;
		if (!task->shouldDestroy) return;
		task->Destroy();
		gBucketPool.get()->Free(task);
	}
};

template <typename T>
struct DBTask : public IDBTask
{
	void Destroy() override
	{
		static_cast<T*>(this)->~T();
	}
};

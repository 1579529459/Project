#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 	// 慢开始反馈调节算法
	// 1、最开始不会一次向central cache一次批量要太多，因为要太多了可能用不完
	// 2、如果你不要这个size大小内存需求，那么batchNum就会不断增长，直到上限
	// 3、size越大，一次向central cache要的batchNum就越小
	// 4、size越小，一次向central cache要的batchNum就越大
	
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (_freeLists[index].MaxSize() == batchNum)
	{
		_freeLists[index].MaxSize() += 1;
	}
	void* start = nullptr;
	void* end = nullptr;
	//实际获得数量;
	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 1);
	
	if (actualNum == 1)
	{
		assert(start == end);
		return start;
	}
	else
	{
		//start被申请走了 !多余的!NextObj(start)和之后的挂进去;
		_freeLists[index].PushRange(NextObj(start), end);
		return start;
	}

}

void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	if (!_freeLists[index].Empty())
	{
		return _freeLists[index].Pop();
	}
	else
	{
		return FetchFromCentralCache(index, alignSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(ptr);
	assert(size <= MAX_BYTES);

	// 找对映射的自由链表桶，对象插入进入
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);
}
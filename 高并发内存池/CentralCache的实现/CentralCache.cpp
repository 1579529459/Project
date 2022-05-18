#include"CentralCache.h"

CentralCache CentralCache::_sInst;

// 获取一个非空的span
Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size)
{
	//...
	
	return list.Head();
}

// 从中心缓存获取一定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	
	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	// 从span中获取batchNum个对象
	// 如果不够batchNum个，有多少拿多少
	
	//!!巧妙地试用输出型参数 start 和 end  将central_cache和thread_cache联系起来了;
	start = span->_freeList;
	end = start;

	size_t n = 1;
	for (int i = 0; i < batchNum - 1; i++)
	{
		end = NextObj(end);
		if (NextObj(end) == nullptr)
			break;
		n++;
	}
	/*size_t i = 0;
	while (i < batchNum - 1 && NextObj(end) != nullptr)
	{
		end = NextObj(end);
		++i;
		++n;
	}*/
	
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;

	_spanLists[index]._mtx.unlock();
	return n;
}


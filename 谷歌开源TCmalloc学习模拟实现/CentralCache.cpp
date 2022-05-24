#include "CentralCache.h"
#include "PageCache.h"
//#include "PageCache.h"

CentralCache CentralCache::_sInst;

// 获取一个非空的span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)//参数list是某一个确定的index桶
{
	// 查看当前的spanlist中是否有还有未分配对象的span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;//这里不需要改it的span中的属性，因为等最后分给threadcache了obj以后 才算其中的内存被分出去了 里面的usecount等才需要改；
		}
		else
		{
			it = it->_next;
		}
	}
	// 走到这里说没有空闲span了，只能找page cache要
	// 先把central cache的桶锁解掉，这样如果其他线程释放内存对象回来，不会阻塞（你要从page申请了 不能别的线程在这个桶释放）
	list._mtx.unlock();

	
	PageCache::GetInstance()->_pageMtx.lock();//整个pagecache结构可能会从index1~index129挨个操作每个桶 因此需要上大锁;
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;//分跟central的span标记已使用因为马上就要切分给obj用了
	span->_objSize = size;//每个span挂的固定小内存块obj大小size
	PageCache::GetInstance()->_pageMtx.unlock();

	// 对获取span进行切分，不需要加锁，因为这会其他线程访问不到这个span
	// 计算span的大块内存的起始地址和大块内存的大小(字节数)
	char* start = (char*)(span->_pageId << PAGE_SHIFT);//这里的_pageId是从底层按页申请内存的时候地址转换来的，现在需要用地址就转换回去;
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	// 把大块内存切成自由链表链接起来
	// 1、先切一块下来去做头，方便尾插(尾插原因，切出来一般是连续的，那么尾插给到span上挂小obj也是连续，提高内存命中率)
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	int i = 1;

	while (start < end)
	{
		++i;
		NextObj(tail) = start;
		tail = NextObj(tail); // tail = start;
		start += size;
	}

	NextObj(tail) = nullptr;

	// 切好span以后，需要把span挂到桶里面去的时候，再加锁
	list._mtx.lock();
	list.PushFront(span);
	return span;
}

// 从中心缓存获取一定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();//上桶锁

	Span* span = GetOneSpan(_spanLists[index],size);
	assert(span);
	assert(span->_freeList);
	// 从span中获取batchNum个对象
	// 如果不够batchNum个，有多少拿多少
	start = span->_freeList;
	end = start;
	int n = 1;//n为实际拿到的数量，start直接给了所以起始值为1;
		for (int i = 0; i < batchNum - 1; i++)
		{
			if (NextObj(end) == nullptr) break;
			end = NextObj(end);
			++n;
		}
	//span被切出去给obj使用了  span的一些属性得改变了;
	span->_useCount += n;
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	_spanLists[index]._mtx.unlock();
	return n;
	
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);

		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		//小obj头插入span中的_freeList 模拟一个归还的过程
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		
		// 说明span的切分出去的所有小块内存都回来了
		// 这个span就可以再回收给page cache，pagecache可以再尝试去做前后页的合并
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// 释放span给page cache时，使用page cache的锁就可以了
			// 这时把桶锁解掉 不影响其他线程对该index的central操作;
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);//还给page和page是否需要合并其中的span减少内存碎片都在这函数里
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock();
		}

		start = next;
	}

	_spanLists[index]._mtx.unlock();
}
#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

//tcmallo
static void* ConcurrentAlloc(size_t size)
{
	//一次申请内存大于thread最大的256kb，则直接向page层按页直接申请，不需要经过Thread层;
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);
		span->_objSize = size;//这个span就是为了_objsize单位内存而申请的
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		// 通过TLS 每个线程无锁的获取自己的专属的ThreadCache对象
		if (pTLSThreadCache == nullptr)
		{
			static ObjectPool<ThreadCache> tcPool;//只需要一个tcpool，用于调用New，所以设置成静态;
			//pTLSThreadCache = new ThreadCache;
			pTLSThreadCache = tcPool.New();
		}

		return pTLSThreadCache->Allocate(size);
	}
}

//tcfree
static void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;

	if (size > MAX_BYTES)//证明这个Span直接是按页从page申请的_objsize>MAX_SIZE，没经过thread，那么直接ReleaseSpanToPageCache
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}
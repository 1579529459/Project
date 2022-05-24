#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"

//tcmallo
static void* ConcurrentAlloc(size_t size)
{
	//һ�������ڴ����thread����256kb����ֱ����page�㰴ҳֱ�����룬����Ҫ����Thread��;
	if (size > MAX_BYTES)
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);
		span->_objSize = size;//���span����Ϊ��_objsize��λ�ڴ�������
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}
	else
	{
		// ͨ��TLS ÿ���߳������Ļ�ȡ�Լ���ר����ThreadCache����
		if (pTLSThreadCache == nullptr)
		{
			static ObjectPool<ThreadCache> tcPool;//ֻ��Ҫһ��tcpool�����ڵ���New���������óɾ�̬;
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

	if (size > MAX_BYTES)//֤�����Spanֱ���ǰ�ҳ��page�����_objsize>MAX_SIZE��û����thread����ôֱ��ReleaseSpanToPageCache
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
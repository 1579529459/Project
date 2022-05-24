#include "CentralCache.h"
#include "PageCache.h"
//#include "PageCache.h"

CentralCache CentralCache::_sInst;

// ��ȡһ���ǿյ�span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)//����list��ĳһ��ȷ����indexͰ
{
	// �鿴��ǰ��spanlist���Ƿ��л���δ��������span
	Span* it = list.Begin();
	while (it != list.End())
	{
		if (it->_freeList != nullptr)
		{
			return it;//���ﲻ��Ҫ��it��span�е����ԣ���Ϊ�����ָ�threadcache��obj�Ժ� �������е��ڴ汻�ֳ�ȥ�� �����usecount�Ȳ���Ҫ�ģ�
		}
		else
		{
			it = it->_next;
		}
	}
	// �ߵ�����˵û�п���span�ˣ�ֻ����page cacheҪ
	// �Ȱ�central cache��Ͱ�������������������߳��ͷ��ڴ���������������������Ҫ��page������ ���ܱ���߳������Ͱ�ͷţ�
	list._mtx.unlock();

	
	PageCache::GetInstance()->_pageMtx.lock();//����pagecache�ṹ���ܻ��index1~index129��������ÿ��Ͱ �����Ҫ�ϴ���;
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;//�ָ�central��span�����ʹ����Ϊ���Ͼ�Ҫ�зָ�obj����
	span->_objSize = size;//ÿ��span�ҵĹ̶�С�ڴ��obj��Сsize
	PageCache::GetInstance()->_pageMtx.unlock();

	// �Ի�ȡspan�����з֣�����Ҫ��������Ϊ��������̷߳��ʲ������span
	// ����span�Ĵ���ڴ����ʼ��ַ�ʹ���ڴ�Ĵ�С(�ֽ���)
	char* start = (char*)(span->_pageId << PAGE_SHIFT);//�����_pageId�Ǵӵײ㰴ҳ�����ڴ��ʱ���ַת�����ģ�������Ҫ�õ�ַ��ת����ȥ;
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes;

	// �Ѵ���ڴ��г�����������������
	// 1������һ������ȥ��ͷ������β��(β��ԭ���г���һ���������ģ���ôβ�����span�Ϲ�СobjҲ������������ڴ�������)
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

	// �к�span�Ժ���Ҫ��span�ҵ�Ͱ����ȥ��ʱ���ټ���
	list._mtx.lock();
	list.PushFront(span);
	return span;
}

// �����Ļ����ȡһ�������Ķ����thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();//��Ͱ��

	Span* span = GetOneSpan(_spanLists[index],size);
	assert(span);
	assert(span->_freeList);
	// ��span�л�ȡbatchNum������
	// �������batchNum�����ж����ö���
	start = span->_freeList;
	end = start;
	int n = 1;//nΪʵ���õ���������startֱ�Ӹ���������ʼֵΪ1;
		for (int i = 0; i < batchNum - 1; i++)
		{
			if (NextObj(end) == nullptr) break;
			end = NextObj(end);
			++n;
		}
	//span���г�ȥ��objʹ����  span��һЩ���Եøı���;
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
		//Сobjͷ����span�е�_freeList ģ��һ���黹�Ĺ���
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;
		
		// ˵��span���зֳ�ȥ������С���ڴ涼������
		// ���span�Ϳ����ٻ��ո�page cache��pagecache�����ٳ���ȥ��ǰ��ҳ�ĺϲ�
		if (span->_useCount == 0)
		{
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// �ͷ�span��page cacheʱ��ʹ��page cache�����Ϳ�����
			// ��ʱ��Ͱ����� ��Ӱ�������̶߳Ը�index��central����;
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);//����page��page�Ƿ���Ҫ�ϲ����е�span�����ڴ���Ƭ�����⺯����
			PageCache::GetInstance()->_pageMtx.unlock();

			_spanLists[index]._mtx.lock();
		}

		start = next;
	}

	_spanLists[index]._mtx.unlock();
}
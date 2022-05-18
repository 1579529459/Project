#include"CentralCache.h"

CentralCache CentralCache::_sInst;

// ��ȡһ���ǿյ�span
Span* CentralCache::GetOneSpan(SpanList& list, size_t byte_size)
{
	//...
	
	return list.Head();
}

// �����Ļ����ȡһ�������Ķ����thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	size_t index = SizeClass::Index(size);
	_spanLists[index]._mtx.lock();
	
	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	// ��span�л�ȡbatchNum������
	// �������batchNum�����ж����ö���
	
	//!!�������������Ͳ��� start �� end  ��central_cache��thread_cache��ϵ������;
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


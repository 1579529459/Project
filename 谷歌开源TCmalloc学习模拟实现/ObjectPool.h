#pragma once
#include"Common.h"

	

template<class T>
class ObjectPool
{
	char* _memory = nullptr; // 指向大块内存的指针
	size_t _remainBytes = 0; // 大块内存在切分过程中剩余字节数
	void* _freeList = nullptr; // 还回来过程中链接的自由链表的头指针

public:
	T* New()
	{
		T* obj = nullptr;
		//如果之前有申请的空间被释放，那就先从free了的空间拿;(内存池技术)
		if (_freeList != nullptr)	
		{
			//头删
			void* next = *((void**)_freeList);//*((void**)_freeList)：取其前4or8个字节操作
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			if (_remainBytes < sizeof(T))
			{	
				// 剩余内存不够一个对象大小时，则重新开大块空间
				_remainBytes = 128 * 1024;
				//_memory = (char*)malloc(_remainBytes);使用SystemAlloc完全脱离malloc;
				_memory = (char*)SystemAlloc(_remainBytes >> 13);

				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
				
			}
			//防止某个T类还没当前系统下一个指针大小大 那么就装不下后一个的地址了，这里做特殊处理;至少保证一个对象内足够装的下一个指针大小;
			int Objsize = sizeof(T) < sizeof(void*) ? sizeof(void*): sizeof(T);

			obj = (T*)_memory;
			_memory += Objsize;
			_remainBytes -= Objsize;

		}
		// 定位new，显示调用T的构造函数初始化
		new(obj)T;
		return obj;
	}

	void Delete(T* obj)
	{
		obj->~T();
		//头插(此处不是真正的删除，而是标志为未使用，挂入自由链表)
		*(void**)obj = _freeList;
		_freeList = obj;
		
	}
}; 
	
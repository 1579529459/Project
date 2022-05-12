#pragma once
#define _CRT_SECURE_NO_WARNINGS 1
#include<iostream>
using namespace std;

#include<iostream>

using namespace std;

#ifdef _WIN32
#include<windows.h>
#else
// 
#endif

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}


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
		//先从free了的空间拿;
		if (_freeList != nullptr)
		{
			//头删
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			if (_remainBytes < sizeof(T))
			{	
				// 剩余内存不够一个对象大小时，则重新开大块空间
				_remainBytes = 128 * 1024;
				//_memory = (char*)malloc(_remainBytes);
				_memory = (char*)SystemAlloc(_remainBytes >> 13);

				//没开出来抛异常
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
				
			}
			//防止某个T类还没当前系统下指针大 那么就装不下后一个的地址了;
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
		//头插
		*(void**)obj = _freeList;
		_freeList = obj;
		
	}
};




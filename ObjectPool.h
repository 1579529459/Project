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

// ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}


template<class T>
class ObjectPool
{
	char* _memory = nullptr; // ָ�����ڴ��ָ��
	size_t _remainBytes = 0; // ����ڴ����зֹ�����ʣ���ֽ���
	void* _freeList = nullptr; // ���������������ӵ����������ͷָ��

public:
	T* New()
	{
		T* obj = nullptr;
		//�ȴ�free�˵Ŀռ���;
		if (_freeList != nullptr)
		{
			//ͷɾ
			void* next = *((void**)_freeList);
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			if (_remainBytes < sizeof(T))
			{	
				// ʣ���ڴ治��һ�������Сʱ�������¿����ռ�
				_remainBytes = 128 * 1024;
				//_memory = (char*)malloc(_remainBytes);
				_memory = (char*)SystemAlloc(_remainBytes >> 13);

				//û���������쳣
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
				
			}
			//��ֹĳ��T�໹û��ǰϵͳ��ָ��� ��ô��װ���º�һ���ĵ�ַ��;
			int Objsize = sizeof(T) < sizeof(void*) ? sizeof(void*): sizeof(T);

			obj = (T*)_memory;
			_memory += Objsize;
			_remainBytes -= Objsize;

		}
		// ��λnew����ʾ����T�Ĺ��캯����ʼ��
		new(obj)T;
		return obj;

	}

	void Delete(T* obj)
	{
		obj->~T();
		//ͷ��
		*(void**)obj = _freeList;
		_freeList = obj;
		
	}
};




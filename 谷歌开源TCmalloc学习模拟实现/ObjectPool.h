#pragma once
#include"Common.h"

	

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
		//���֮ǰ������Ŀռ䱻�ͷţ��Ǿ��ȴ�free�˵Ŀռ���;(�ڴ�ؼ���)
		if (_freeList != nullptr)	
		{
			//ͷɾ
			void* next = *((void**)_freeList);//*((void**)_freeList)��ȡ��ǰ4or8���ֽڲ���
			obj = (T*)_freeList;
			_freeList = next;
		}
		else
		{
			if (_remainBytes < sizeof(T))
			{	
				// ʣ���ڴ治��һ�������Сʱ�������¿����ռ�
				_remainBytes = 128 * 1024;
				//_memory = (char*)malloc(_remainBytes);ʹ��SystemAlloc��ȫ����malloc;
				_memory = (char*)SystemAlloc(_remainBytes >> 13);

				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
				
			}
			//��ֹĳ��T�໹û��ǰϵͳ��һ��ָ���С�� ��ô��װ���º�һ���ĵ�ַ�ˣ����������⴦��;���ٱ�֤һ���������㹻װ����һ��ָ���С;
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
		//ͷ��(�˴�����������ɾ�������Ǳ�־Ϊδʹ�ã�������������)
		*(void**)obj = _freeList;
		_freeList = obj;
		
	}
}; 
	
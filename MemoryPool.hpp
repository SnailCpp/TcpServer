#ifndef _MEMORY_POOL_H_
#define _MEMORY_POOL_H_

#include <map>

size_t RoundUp(size_t bytes)
{
	return (((bytes) + sizeof(int) - 1) & ~(sizeof(int) - 1));
}

union Chunk
{
	union Chunk* _freeListLink;
	char client_data[1];	
};

class MemoryAllocBase
{
	friend class MemoryPool;
public:
	MemoryAllocBase() : _bytes(0) {}
	~MemoryAllocBase() = default;

	virtual void* Allocate() = 0;
	virtual void Deallocate(void* p) = 0;

protected:
	size_t _bytes;
};

template<int bytes>
class MemoryAlloc : public MemoryAllocBase
{
public:
	MemoryAlloc() : _free_list(nullptr)
	{
		_bytes = RoundUp(bytes);
	}

	~MemoryAlloc() = default;	

	virtual void* Allocate() override
	{
		if (nullptr == _free_list)
		{
			 return Refill();
		}
		else
		{
			Chunk* first = _free_list;
			_free_list = first->_freeListLink;
			return first;
		}

		return nullptr;
	}

	virtual void Deallocate(void* p) override
	{		
		Chunk* chunk = (Chunk*)p;
		chunk->_freeListLink = _free_list;
		_free_list = chunk;
	}

private:	
	void* Refill()
	{
		// 分配nchunks个对象的大小
		size_t nchunks = 20;
		char* chunk = (char*)malloc(_bytes * nchunks);
		
		// 挂内存块
		Chunk* cur, *next;
		cur = (Chunk*)(chunk + _bytes);
		_free_list = cur;
		for (size_t i = 1; i < nchunks - 1; ++i)
		{
			next = (Chunk*)((char*)cur + _bytes);
			cur->_freeListLink = next;
			cur = next;
		}
		cur->_freeListLink = nullptr;
		return chunk;
	}

private:
	Chunk* _free_list;	
};

class MemoryPool
{	
public:	
	template <class T>
	class Resiter
	{
	public:
		Resiter()
		{
			MemoryPool::Instance().RegisterAlloctor(&_p);
		}

	private:
		T _p;
	};

	~MemoryPool() = default;
	static MemoryPool& Instance()
	{
		static MemoryPool inst;
		return inst;
	}	

	void RegisterAlloctor(MemoryAllocBase* p)
	{
		if(_mem_count < 1024 && p) _mem_map[_mem_count++] = p;
	}

	void * Allocate(size_t n)
	{
		for (int i = 0; i < _mem_count; ++i)
		{
			if (n <= _mem_map[i]->_bytes)
			{
				return _mem_map[i]->Allocate();
			}
		}	

		return nullptr;
	}

	void Deallocate(void* p, size_t n)
	{
		for (int i = 0; i < _mem_count; ++i)
		{
			if (n <= _mem_map[i]->_bytes)
			{
				_mem_map[i]->Deallocate(p);
				return;
			}
		}
	}

private:
	MemoryPool():  _mem_count(0) {}	

private:
	MemoryAllocBase* _mem_map[1024];
	int				_mem_count;
};

#define MEMORYPOOL_REGISTER(CLASS, BYTES) MemoryPool::Resiter<CLASS> MemoryAlloc_##BYTES

#endif // 
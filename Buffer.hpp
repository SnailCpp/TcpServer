#ifndef _BUFFER_HPP_
#define _BUFFER_HPP_

#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

// »º³å¶ÓÁÐ
class Buffer
{
public:
	Buffer(size_t cap = 0) : _ptr(nullptr), _size(0), _cap(cap)
	{
		if (cap >= 0) {
			_ptr = (char *)calloc(cap, sizeof(char));
		}
	}

	~Buffer()
	{
		this->Free();
	}

	void Free()
	{
		if (_ptr) free(_ptr);
		_ptr = 0;
		_size = 0;
		_cap = 0;
	}
	
	int Realloc(uint32_t size)
	{
		if (size <= 0) return -1;
		uint32_t cap = (uint32_t)ceil((_cap + size) / 1024.0) * 10240;
		if(cap <= _cap) return -2;
		char* ptr = (char *)realloc(_ptr, cap);
		if (!ptr) return -3;
		_ptr = ptr;
		_cap = cap;
		return 0;
	}

	bool Push(const char * bytes, uint32_t size)
	{
		if (!bytes || size <= 0) return false;
		uint32_t freeSize = this->GetFreeSize();
		if (freeSize < size)
		{
			if (Realloc(size - freeSize)) return false;
		}

		memmove(_ptr + _size, bytes, size);
		_size += size;
		return true;
	}

	bool Pop(uint8_t * destination, uint32_t size)
	{
		if (size > _size) return false;
		memmove(destination, _ptr, size);	
		memmove(_ptr, _ptr + size, _size - size);
		_size -= size;
		return true;
	}

	uint32_t GetFreeSize()
	{
		return _cap - _size;
	}

	uint32_t Size()
	{
		return _size;
	}

	bool IsEmpty()
	{
		return (0 == _size);
	}

	bool IsFull()
	{
		return (_cap == _size);
	}

	uint32_t GetCapacity()
	{
		return _cap;
	}

	char* Begin()
	{
		return _ptr;
	}

	char* End()
	{
		return _ptr + _size;
	}	

	int RemoveTop(uint32_t size)
	{
		if (size > _size) return -1;
		memmove(_ptr, _ptr + size, _size - size);
		_size -= size;
		return 0;
	}

	int Resize(uint32_t size)
	{	
		if (size > _cap) return -1;
		_size = size;
		return 0;
	}

private:
	char *_ptr;
	uint32_t _size;
	uint32_t _cap;
};

#endif
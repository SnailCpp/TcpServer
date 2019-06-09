#ifndef _ALLOCATOR_HPP_
#define _ALLOCATOR_HPP_

#include "MemoryPool.hpp"

void* operator new(size_t size)
{
	return MemoryPool::Instance().Allocate(size);
}

void operator delete(void* pdead, size_t size)
{
	MemoryPool::Instance().Deallocate(pdead, size);
}

void* operator new[](size_t size)
{
	return MemoryPool::Instance().Allocate(size);
}

void operator delete[](void*pdead, size_t size)
{
	MemoryPool::Instance().Deallocate(pdead, size);
}

#endif
#ifndef __MM_WRAPPER_H__
#define __MM_WRAPPER_H__

#include <stdlib.h>

using malloc_function_t = void* (*)(size_t);
using calloc_function_t = void* (*)(size_t, size_t);
using free_function_t = void (*)(void *ptr);
using aligned_alloc_function_t =  void* (*)(size_t alignment, size_t size);
using realloc_function_t = void * (*)(void *ptr, size_t size);
using memalign_function_t = void * (*)(size_t alignment, size_t size);
#endif // __MM_WRAPPER_H__

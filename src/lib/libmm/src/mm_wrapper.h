#include <stdlib.h>

extern "c"
{
  void * malloc(size_t size);
  void free(void * ptr);
}

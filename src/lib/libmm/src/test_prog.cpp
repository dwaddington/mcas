#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

//#define TEST_LOAD

int main()
{
#ifdef TEST_LOAD
  static const char * PLUGIN_PATH = "/home/danielwaddington/mcas/build/dist/lib/libmm-plugin-jemalloc.so";
  void * mod = dlopen(PLUGIN_PATH, RTLD_NOW | RTLD_DEEPBIND);
  if(!mod) printf("Error: %s\n", dlerror());
#endif
  
  printf("Test prog.\n");

  {
    size_t s = 1024 * 2048;
    void * p = malloc(s);
    printf("result of malloc: p=%p\n", p);
    memset(p, 0, s);
    free(p);
  }

  {
    void * p = calloc(32, 128);
    printf("result of calloc: p=%p\n", p);
    free(p);
  }
  

  printf("Done.\n");

  return 0;
}

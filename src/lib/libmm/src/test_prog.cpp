#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main()
{
  printf("Test prog.\n");

  {
    void * p = malloc(128);
    printf("malloc: p=%p\n", p);
    memset(p, 0, 128);
    free(p);
  }

  {
    void * p = calloc(32, 128);
    printf("calloc: p=%p\n", p);
    free(p);
  }
  

  printf("Done.\n");

  return 0;
}

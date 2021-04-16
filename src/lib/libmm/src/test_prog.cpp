#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main()
{
  printf("Test prog.\n");
  void * p = malloc(128);
  printf("p=%p\n", p);
  memset(p, 0, 128);
  free(p);

  printf("Done.\n");
  return 0;
}

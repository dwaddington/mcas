#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main()
{
  printf("Test prog.\n");
  void * p = malloc(128);
  memset(p, 0, 128);
  free(p);
  
  return 0;
}

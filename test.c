#include <stdio.h>
#include <stdlib.h> // malloc и иже с ним

#include <unistd.h>
#include <sys/syscall.h>

int main(){
  void * ptr1 = malloc(32);
  void * ptr2 = malloc(32);
  free(ptr2);
  return 0;
}
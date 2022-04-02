#include <iostream>
#include "uthreads.h"

long wait_time= 100000000;

void someFun1 ()
{
  std::cout << "My program is running 111" << std::endl;
  int i = 0;
  while (i < wait_time)
    { if (i == wait_time/2) uthread_sleep (10);
      i++; }
  uthread_terminate (1);
}

void someFun2 ()
{
  std::cout << "My program is running 222" << std::endl;
  int i = 0;
  while (i < wait_time)
    { i++; }
  uthread_terminate (2);
}

int main ()
{
//  std::cout << "Hello, World!" << std::endl;


  uthread_init (50);
  uthread_spawn (someFun1);
  uthread_spawn (someFun2);
  int i = 0;
  while (i < wait_time)
    { i++; }
  uthread_terminate (0);
  return 0;
}




#include <iostream>
#include "uthreads.h"

void someFun1(){
  std::cout << "My program is running 111" << std::endl;
  while(1);
  uthread_terminate (1);
}

void someFun2(){
  std::cout << "My program is running 222" << std::endl;
  while(1);
  uthread_terminate (2);
}

int main ()
{
//  std::cout << "Hello, World!" << std::endl;


  uthread_init (500);
  uthread_spawn (someFun1);
  uthread_spawn (someFun2);
  while(1);

  return 0;
}




#include <iostream>
#include "uthreads.h"

void someFun(){
  std::cout << "My program is running" << std::endl;
}

int main ()
{
//  std::cout << "Hello, World!" << std::endl;


  uthread_init (500);
  uthread_spawn (someFun);

  return 0;
}




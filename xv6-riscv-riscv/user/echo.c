#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 调用方式：大部分的程序都忽略第一个参数，这个参数惯例上是程序的名字，此例是 echo。
// char *argv[3];
// argv[0] = "echo";
// argv[1] = "hello";
// argv[2] = 0;
// exec("/bin/echo", argv);

int
main(int argc, char *argv[])
{
  int i;

  for(i = 1; i < argc; i++){
    write(1, argv[i], strlen(argv[i]));
    if(i + 1 < argc){
      write(1, " ", 1);
    } else {
      write(1, "\n", 1);
    }
  }
  exit(0);
}

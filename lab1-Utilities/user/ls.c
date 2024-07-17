#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  //O_RDONLY 是一个标志，表示以只读模式打开文件
  //如果 open 函数成功，fd 将是一个非负整数，表示文件的描述符。如果 open 失败，返回值将是 -1。
  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  //fstat函数用于获取文件描述符所引用的文件的状态信息，并将这些信息存储在一个struct stat结构体中
  //如果 fstat 调用成功，返回值为 0。如果 fstat 调用失败，返回值为 -1，
  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_DEVICE:
  case T_FILE:
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;

  //DIRSIZ文件名的最短长度
  case T_DIR:
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;
  //命令行中只有ls，没有目录，直接输出当前文件目录的内容。
  if(argc < 2){
    ls(".");
    exit(0);
  }
  //打印多个目录或文件的内容，并合并在一起输出。
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}

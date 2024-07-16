/*
int pipe（int fds[2]）；
参数：
fd[0]是读取管道末端。
fd[1]将是管道写入端的 fd。
返回：成功时返回 0。
错误时返回-1。

read(int fd, void *buf, int count);
fd为相应的文件描述符；buf为用户给定的数据缓冲区，由count值决定其大小（字节数）。

write(int fd, const void *buf, sint count);
buf为需要输出的缓冲区，由用户给定。cout为最大输出字节数（buf的大小，字节数）。

getpid() 返回当前进程的进程 ID。
*/

#include "kernel/types.h"
#include "user/user.h"

#define MSGSIZE 1

int main(int argc, char const *argv[])
{
    char msg = 'A';
    int fd[2];
    char buff[MSGSIZE];
    if (pipe(fd) < 0)
        exit(1);

    int pid = fork(); // 创建进程
    // 错误情况
    if (pid < 0)
    {
        printf("error!\n");
        // 关闭读取、写入端
        close(fd[0]);
        close(fd[1]);
        exit(1);
    }
    else if (0 == pid) // 子进程
    {
        read(fd[0], buff, MSGSIZE);
        printf("%d: received ping\n", getpid());
        write(fd[1], buff, MSGSIZE);
        exit(0);
    }
    else // 父进程
    {
        write(fd[1], &msg, MSGSIZE);
        read(fd[0], buff, MSGSIZE);
        printf("%d: received pong\n", getpid());
        exit(0);
    }
    close(fd[0]);
    close(fd[1]);
    exit(0);
}
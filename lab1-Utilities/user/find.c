#include "kernel/types.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "user/user.h"

void find(char *path, const char *filename)
{
    //path 在path目录下查找指定文件
    //filename 查找名称为filename的文件
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    // 打开失败
    if ((fd = open(path, 0)) < 0) {
        printf("find: cannot open %s\n", path);
        return;
    }
    // 描述失败
    if (fstat(fd, &st) < 0) {
        printf("find: cannot fstat %s\n", path);
        close(fd);
        return;
    }
    //find的第一个参数必须是目录
    if (st.type != T_DIR) {
        printf("usage: find <DIRECTORY> <filename>\n");
        return;
    }
    strcpy(buf, path);
    
    p = buf + strlen(buf);
    //p指向最后一个'/'的下一个位置
    *p++ = '/';
    //read 函数用于从文件描述符中读取数据
    //fd：要读取的文件描述符。buf：用于存储读取数据的缓冲区。count：要读取的字节数。
    while (read(fd, &de, sizeof de) == sizeof de) {
        //这个条目是无效的或者是未使用的
        if (de.inum == 0)
            continue;
        //添加路径名称
        memmove(p, de.name, DIRSIZ);
        //补充尾0
        p[DIRSIZ] = 0;
        //stat函数用于获取文件描述符所引用的文件的状态信息，并将这些信息存储在一个struct stat结构体中
        if (stat(buf, &st) < 0) {
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        //防止在.和..目录中递归
        if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0)
            find(buf, filename);
        else if (strcmp(filename, p) == 0)
            printf("%s\n", buf);
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    // 输入参数错误
    if (argc != 3) {
        printf("error! usage: find <directory> <filename>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    exit(0);
}
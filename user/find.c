#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 从一个完整路径中提取出最后的文件名部分
// 例如: "a/b/c" -> "c", "." -> "."
char* basename(char *path) {
    static char buf[DIRSIZ+1];
    char *p;

    // 从路径末尾向前找到第一个'/'
    for(p=path+strlen(path); p >= path && *p != '/'; p--);
    p++; // p现在指向文件名的第一个字符

    // 如果文件名太长，则截断
    if(strlen(p) >= DIRSIZ)
        return p;
    
    // 复制文件名到一个静态缓冲区中并返回
    // 这是为了避免直接返回指向原路径的指针，增加安全性
    strcpy(buf, p);
    return buf;
}

void find(char *path, char *filename) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // 尝试打开路径，如果失败则返回
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 获取文件状态，如果失败则关闭文件描述符并返回
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 根据文件类型进行处理
    switch (st.type) {
        // 如果是文件类型
        case T_FILE:
            // 比较其基本名称是否与目标文件名相同
            if (strcmp(basename(path), filename) == 0) {
                printf("%s\n", path);
            }
            break;

        // 如果是目录类型
        case T_DIR:
            // 检查路径长度是否会超出缓冲区
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
                fprintf(2, "find: path too long\n");
                break;
            }
            // 将当前路径复制到缓冲区
            strcpy(buf, path);
            p = buf + strlen(buf);
            // 在路径末尾添加'/'，为拼接子目录/文件名做准备
            *p++ = '/';

            // 读取目录中的每一个条目
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                // 跳过无效条目和特殊目录"."、".."
                if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
                    continue;
                }
                // 将目录条目的名称拼接到路径末尾
                strcpy(p, de.name);
                // 对拼接好的新路径进行递归调用
                find(buf, filename);
            }
            break;
    }
    // 完成操作后关闭文件描述符
    close(fd);
}

int main(int argc, char *argv[]) {
    // 检查命令行参数数量是否正确
    if (argc != 3) {
        fprintf(2, "Usage: find <directory> <filename>\n");
        exit(1);
    }
    // 开始递归查找
    find(argv[1], argv[2]);
    exit(0);
}

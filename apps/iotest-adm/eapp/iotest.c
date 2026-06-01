#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FILENAME "data.tmp"
#define BLOCK_SZ 4096
#define BLOCK_N 1

int check_blk(unsigned char* buf1, unsigned char* buf2, size_t cnt) {
    for (size_t i = 0; i < cnt; i++) {
        if (buf1[i] != buf2[i]) {
            return 1;
        }
    }
    return 0;
}

int main() {
    int fd;

    printf("## I/O syscall delegation test\n");
    printf("## Size of block: %d\n## Block r/w count: %d\n\n", BLOCK_SZ,
           BLOCK_N);

    unsigned char* buf = (unsigned char*)malloc(sizeof(char) * BLOCK_SZ);
    for (int i = 0; i < BLOCK_SZ; i++) {
        buf[i] = (unsigned char)(rand() % UCHAR_MAX);
    }

    printf("Step 1: writing to the file...");
    if ((fd = open(FILENAME, O_WRONLY | O_TRUNC | O_CREAT, 0644)) == -1) {
        printf("failed to open file: %s\n", FILENAME);
        return 1;
    }
    for (int i = 0; i < BLOCK_N; i++) {
        if (write(fd, buf, BLOCK_SZ) != BLOCK_SZ) {
            printf("ERROR\n");
            return 1;
        }
    }
    close(fd);
    printf("OK\n");

    printf("Step 2: reading from the file...");
    if ((fd = open(FILENAME, O_RDONLY)) == -1) {
        printf("failed to open file: %s\n", FILENAME);
        return 1;
    }
    unsigned char* read_buf = (unsigned char*)malloc(sizeof(char) * BLOCK_SZ);
    for (int i = 0; i < BLOCK_N; i++) {
        if (read(fd, read_buf, BLOCK_SZ) != BLOCK_SZ) {
            printf("ERROR\n");
            return 1;
        }
        if (check_blk(buf, read_buf, BLOCK_SZ)) {
            printf("ERROR: read block is unmatched (pos: %d)\n", i);
            return 1;
        }
    }
    free(buf);
    free(read_buf);
    close(fd);
    printf("OK\n");

    return 0;
}

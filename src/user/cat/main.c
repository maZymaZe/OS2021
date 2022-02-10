#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
char buf[512];
void cat(int fd) {
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
    }
    if (n < 0) {
        printf("cat: read error\n");
        exit(1);
    }
}
int main(int argc, char* argv[]) {
    /* TODO: Lab9 Shell */
    int fd, i;
    if (argc <= 1) {
        cat(0);
        exit(0);
    }
    for (i = 1; i < argc; i++) {
        if ((fd = open(argv[i], 0)) < 0) {
            printf("cat: cannot open %s\n", argv[i]);
            exit(0);
        }
        cat(fd);
        close(fd);
    }
    exit(0);
    return 0;
}

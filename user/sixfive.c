#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define DELIMS "-\r\t\n./,"

void process_file(int fd) {
    char buf[1];
    char num[32];
    int n = 0;

    while (read(fd, buf, 1) == 1) {
        char c = buf[0];
        if (strchr(DELIMS, c)) {
            if (n > 0) {
                num[n] = '\0';
                int val = atoi(num);
                if (val % 5 == 0 || val % 6 == 0)
                    printf("%d\n", val);
                n = 0;
            }
        } else if (c >= '0' && c <= '9') {
            if (n < 31) num[n++] = c;
        } else {
            n = 0;
        }
    }

    if (n > 0) {
        num[n] = '\0';
        int val = atoi(num);
        if (val % 5 == 0 || val % 6 == 0)
            printf("%d\n", val);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "usage: sixfive file...\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(2, "sixfive: cannot open %s\n", argv[i]);
            continue;
        }
        process_file(fd);
        close(fd);
    }
    exit(0);
}
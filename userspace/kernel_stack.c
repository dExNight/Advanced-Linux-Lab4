#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#define DEVICE_PATH "/dev/int_stack"
#define IOCTL_SET_SIZE _IOW('s', 1, int)

void print_usage(const char *prog_name) {
    printf("Usage:\n");
    printf("  %s set-size <size>\n", prog_name);
    printf("  %s push <value>\n", prog_name);
    printf("  %s pop\n", prog_name);
    printf("  %s unwind\n", prog_name);
}

int main(int argc, char *argv[]) {
    int fd;
    int value, size;
    int ret;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return errno;
    }

    if (strcmp(argv[1], "set-size") == 0) {
        if (argc != 3) {
            print_usage(argv[0]);
            close(fd);
            return 1;
        }
        
        size = atoi(argv[2]);
        if (size <= 0) {
            printf("ERROR: size should be > 0\n");
            close(fd);
            return 1;
        }
        
        ret = ioctl(fd, IOCTL_SET_SIZE, &size);
        if (ret < 0) {
            perror("ioctl failed");
            close(fd);
            return errno;
        }
    }
    else if (strcmp(argv[1], "push") == 0) {
        if (argc != 3) {
            print_usage(argv[0]);
            close(fd);
            return 1;
        }
        
        value = atoi(argv[2]);
        ret = write(fd, &value, sizeof(int));
        if (ret < 0) {
            if (errno == ERANGE) {
                printf("ERROR: stack is full\n");
                close(fd);
                return 34;  // Return ERANGE
            }
            perror("write failed");
            close(fd);
            return errno;
        }
    }
    else if (strcmp(argv[1], "pop") == 0) {
        ret = read(fd, &value, sizeof(int));
        if (ret == 0) {
            printf("NULL\n");
        } else if (ret < 0) {
            perror("read failed");
            close(fd);
            return errno;
        } else {
            printf("%d\n", value);
        }
    }
    else if (strcmp(argv[1], "unwind") == 0) {
        while ((ret = read(fd, &value, sizeof(int))) > 0) {
            printf("%d\n", value);
        }
        if (ret < 0) {
            perror("read failed");
            close(fd);
            return errno;
        }
    }
    else {
        print_usage(argv[0]);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
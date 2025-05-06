#ifndef INT_STACK_H
#define INT_STACK_H

#define DEVICE_NAME "int_stack"
#define MAJOR_NUMBER 0

#define IOCTL_SET_SIZE _IOW('s', 1, int)

struct int_stack {
    int *data;
    int top;
    int max_size;
    struct mutex lock;
};

#endif
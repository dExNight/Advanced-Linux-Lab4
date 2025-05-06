#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include "int_stack.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amir Gubaidullin");
MODULE_DESCRIPTION("Integer Stack Character Device");
MODULE_VERSION("0.1");

static dev_t dev_num;
static struct cdev c_dev;
static struct class *cl;
static struct int_stack *stack;

// Function prototypes
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);
static long device_ioctl(struct file *, unsigned int, unsigned long);

static struct file_operations fops = {
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
    .unlocked_ioctl = device_ioctl,
    .owner = THIS_MODULE,
};

static int device_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    int value;
    int ret;
    
    if (length < sizeof(int))
        return -EINVAL;

    mutex_lock(&stack->lock);
    
    // Pop operation
    if (stack->top == -1) {
        mutex_unlock(&stack->lock);
        return 0; // Return 0 bytes if stack is empty
    }
    
    value = stack->data[stack->top];
    stack->top--;
    
    mutex_unlock(&stack->lock);
    
    ret = copy_to_user(buffer, &value, sizeof(int));
    if (ret)
        return -EFAULT;
    
    return sizeof(int);
}

static ssize_t device_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset)
{
    int value;
    int ret;
    
    if (length < sizeof(int))
        return -EINVAL;

    ret = copy_from_user(&value, buffer, sizeof(int));
    if (ret)
        return -EFAULT;

    mutex_lock(&stack->lock);
    
    // Push operation
    if (stack->max_size > 0 && stack->top >= stack->max_size - 1) {
        mutex_unlock(&stack->lock);
        return -ERANGE; // Stack is full
    }
    
    stack->top++;
    stack->data[stack->top] = value;
    
    mutex_unlock(&stack->lock);
    
    return sizeof(int);
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int size;
    int ret;
    int *new_data;
    int i, copy_size;
    
    switch (cmd) {
    case IOCTL_SET_SIZE:
        ret = copy_from_user(&size, (int __user *)arg, sizeof(int));
        if (ret)
            return -EFAULT;
        
        if (size <= 0)
            return -EINVAL;
        
        mutex_lock(&stack->lock);
        
        // Allocate new buffer
        new_data = kmalloc(size * sizeof(int), GFP_KERNEL);
        if (!new_data) {
            mutex_unlock(&stack->lock);
            return -ENOMEM;
        }
        
        // Determine how many elements to copy
        copy_size = (stack->top + 1 < size) ? (stack->top + 1) : size;
        
        // Copy existing elements to the new buffer
        if (stack->data && copy_size > 0) {
            for (i = 0; i < copy_size; i++) {
                new_data[i] = stack->data[i];
            }
        }
        
        // Free old buffer if exists
        if (stack->data) {
            kfree(stack->data);
        }
        
        // Update stack data
        stack->data = new_data;
        
        // If new size is smaller than the current number of elements
        // truncate the stack (pop excess elements)
        if (stack->top >= size) {
            stack->top = size - 1;
        }
        
        stack->max_size = size;
        
        mutex_unlock(&stack->lock);
        
        return 0;
        
    default:
        return -EINVAL;
    }
}

static int __init int_stack_init(void)
{
    // Allocate device number
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        pr_err("Failed to allocate device number\n");
        return -1;
    }

    // Create device class
    cl = class_create("chardrv");
    if (IS_ERR(cl)) {
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to create class\n");
        return PTR_ERR(cl);
    }

    // Create device
    if (device_create(cl, NULL, dev_num, NULL, DEVICE_NAME) == NULL) {
        class_destroy(cl);
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to create device\n");
        return -1;
    }

    // Initialize and add cdev
    cdev_init(&c_dev, &fops);
    if (cdev_add(&c_dev, dev_num, 1) == -1) {
        device_destroy(cl, dev_num);
        class_destroy(cl);
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to add cdev\n");
        return -1;
    }

    // Allocate memory for stack structure
    stack = kmalloc(sizeof(struct int_stack), GFP_KERNEL);
    if (!stack) {
        cdev_del(&c_dev);
        device_destroy(cl, dev_num);
        class_destroy(cl);
        unregister_chrdev_region(dev_num, 1);
        pr_err("Failed to allocate stack structure\n");
        return -ENOMEM;
    }

    // Initialize stack
    stack->data = NULL;
    stack->top = -1;
    stack->max_size = 0;
    mutex_init(&stack->lock);

    pr_info("int_stack: module loaded\n");
    return 0;
}

static void __exit int_stack_exit(void)
{
    if (stack) {
        if (stack->data)
            kfree(stack->data);
        kfree(stack);
    }

    cdev_del(&c_dev);
    device_destroy(cl, dev_num);
    class_destroy(cl);
    unregister_chrdev_region(dev_num, 1);
    
    pr_info("int_stack: module unloaded\n");
}

module_init(int_stack_init);
module_exit(int_stack_exit);
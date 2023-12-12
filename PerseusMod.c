#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>

#define DEVICE_NAME "Perseus"
#define CLASS_NAME "CPerseus"

static int majorNumber;
static struct class*  charClass  = NULL;
static struct device* charDevice = NULL;

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

// File operations structure
static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release,
};

// Initialization function
static int __init perseus_init(void) {
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) {
        printk(KERN_ALERT "Failed to register a major number\n");
        return majorNumber;
    }

    // Register the device class
    charClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(charClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(charClass);
    }

    // Register the device driver
    charDevice = device_create(charClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(charDevice)) {
        class_destroy(charClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(charDevice);
    }

    printk(KERN_INFO "Device class created correctly\n");
    return 0;
}

// Cleanup function
static void __exit perseus_exit(void) {
    device_destroy(charClass, MKDEV(majorNumber, 0));
    class_destroy(charClass);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_INFO "Device class destroyed\n");
}

// Open function
static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device opened\n");
    return 0;
}

// Release function
static int device_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device released\n");
    return 0;
}

struct memory_transfer_info {
    pid_t procid;          // Process ID
    void *from_buffer;     // Pointer to the source buffer
    void *to_buffer;       // Pointer to the destination buffer
    size_t buffer_size;    // Size of the buffers
};

// Read function
static ssize_t device_read(struct file *filp, char *buffer, size_t len, loff_t *offset) {
    struct memory_transfer_info info;
    ssize_t ret;

    // First, check that the size of the data being passed from userspace is correct
    if (len != sizeof(info)) {
        printk(KERN_ERR "Invalid size of user data\n");
        return -EINVAL; // Return invalid argument error
    }

    // Now, safely copy the memory_transfer_info struct from userspace to kernel space
    if (copy_from_user(&info, buffer, len)) {
        printk(KERN_ERR "Failed to copy data from userspace\n");
        return -EFAULT; // Return bad address error
    }

    // Implement the memory reading logic here
    struct task_struct *task;
    long bytes_read;

    task = pid_task(find_vpid(info.procid), PIDTYPE_PID);
    if (!task) {
        printk(KERN_ERR "Error: PID not found");
        return -ESRCH; // No such process
    }

    printk(KERN_INFO "To buffer: %d\n", info.to_buffer);
    printk(KERN_INFO "From buffer: %d\n", info.from_buffer);
    printk(KERN_INFO "Buffer size: %d\n", info.buffer_size);

    char *_buffer = kmalloc(info.buffer_size, GFP_KERNEL);
    if (!_buffer) {
        printk(KERN_ERR "Memory allocation failed\n");
        return -ENOMEM; // Out of memory
    }

    bytes_read = access_process_vm(task, info.from_buffer, _buffer, info.buffer_size, 0);

    if (bytes_read < 0) {
        printk(KERN_ERR "Error: Failed to read memory");
        kfree(_buffer);
        return bytes_read;
    }

    printk(KERN_INFO "Bytes read: %d\n", bytes_read);

    // Attempt to copy the read data back to the userspace buffer
    if (copy_to_user(info.to_buffer, _buffer, bytes_read)) {
        printk(KERN_ERR "Failed to copy data to userspace\n");
        kfree(_buffer);
        return -EFAULT; // Return bad address error
    }

    // Assuming we write the full buffer size successfully
    ret = info.buffer_size;

    // Update the file offset
    *offset += ret;

    kfree(_buffer);

    return ret; // Return the number of bytes read from the given process
}

static ssize_t device_write(struct file *filp, const char *buffer, size_t len, loff_t *offset) {
    struct memory_transfer_info info;
    ssize_t ret;

    // Check the size of the data being passed from userspace
    if (len != sizeof(info)) {
        printk(KERN_ERR "Invalid size of user data\n");
        return -EINVAL; // Return invalid argument error
    }

    // Copy the memory_transfer_info struct from userspace to kernel space
    if (copy_from_user(&info, buffer, len)) {
        printk(KERN_ERR "Failed to copy data from userspace\n");
        return -EFAULT; // Return bad address error
    }

    // Implement the memory writing logic here
    struct task_struct *task;
    long bytes_written;

    task = pid_task(find_vpid(info.procid), PIDTYPE_PID);
    if (!task) {
        printk(KERN_ERR "Error: PID not found");
        return -ESRCH; // No such process
    }

    char *_buffer = kmalloc(info.buffer_size, GFP_KERNEL);
    if (!_buffer) {
        printk(KERN_ERR "Memory allocation failed\n");
        return -ENOMEM; // Out of memory
    }

    // Copy the data to be written from the userspace buffer
    if (copy_from_user(_buffer, info.from_buffer, info.buffer_size)) {
        printk(KERN_ERR "Failed to copy write data from userspace\n");
        kfree(_buffer);
        return -EFAULT; // Return bad address error
    }

    bytes_written = access_process_vm(task, info.to_buffer, _buffer, info.buffer_size, 1);

    if (bytes_written < 0) {
        printk(KERN_ERR "Error: Failed to write memory");
        kfree(_buffer);
        return bytes_written;
    }

    ret = bytes_written;

    // Update the file offset
    *offset += ret;

    kfree(_buffer);

    return ret; // Return the number of bytes written to the given process
}

module_init(perseus_init);
module_exit(perseus_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arceas");
MODULE_DESCRIPTION("Stable and secure module for gaming");
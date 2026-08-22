#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Laurent Brusa");
MODULE_DESCRIPTION("A Hello World Kernel Module");

#define MAX_SIZE 256
static char *msg_buffer;
static short message_size = 0;

// Aufgabe 2.2 (cat /dev/fritz_module)
static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
	if (*offset >= message_size) return 0;
	if (len > message_size - *offset) len = message_size - *offset;
	
	if (copy_to_user(buffer, msg_buffer + *offset, len)) return -EFAULT;

	*offset += len;
	return len;
}

// aufgabe 2.1 (echo "text" > /dev/fritz_module)
static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
	if (len >= MAX_SIZE) len = MAX_SIZE - 1;

	if (copy_from_user(msg_buffer, buffer, len)) return -EFAULT;

	msg_buffer[len] = '\0';
	message_size = len;
	return len;
}

static struct file_operations fops = {
	.read = dev_read,
	.write = dev_write,
};

static struct miscdevice my_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "fritz_module",
	.fops = &fops,
};

static int __init my_init(void) {
	// Aufgabe 2.1 Text-Daten vom Userspace in einem dynamischen internen Speicher ablegen
	msg_buffer = kmalloc(MAX_SIZE, GFP_KERNEL);
	if (!msg_buffer) return -ENOMEM;
	misc_register (&my_misc_device);

	printk(KERN_INFO "Hello Kernel: Module loaded successfully (Device at /dev/fritz_module).\n");
	return 0;
}

static void __exit my_exit(void) {
	misc_deregister(&my_misc_device);
	kfree(msg_buffer);
	printk(KERN_INFO "Goodbye Kernel: Module removed!\n");
}

module_init(my_init);
module_exit(my_exit);


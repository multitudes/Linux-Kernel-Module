#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Laurent Brusa");
MODULE_DESCRIPTION("A Hello World Kernel Module");

static int __init hello_init(void){
	printk(KERN_INFO "Hello Kernel: Module loaded successfully\n");
	return 0;
}

static void __exit hello_exit(void) {
	printk(KERN_INFO "Goodbye Kernel: Module removed!\n");
}

module_init(hello_init);
module_exit(hello_exit);


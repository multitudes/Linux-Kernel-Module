
user ten pswd notsure

ssh ten@192.168.64.28

or check with ip a on server in the UTM app

sudo apt update && sudo apt install -y build-essential linux-headers-$(uname -r)

mkdir my_module && cd my_module
nano hello.c


Excellent! Now that you are SSH'd into the VM from your Mac terminal, you have the superpower of copy and paste.

You are ready to tackle the 'Hello World' module. Here is a quick recap of the steps from earlier so you can easily copy and paste them right now:

### 1. Install the Build Tools

Run this in your SSH terminal:

```bash
sudo apt update && sudo apt install -y build-essential linux-headers-$(uname -r)

```

### 2. Write the C Code

Create a folder and the C file:

```bash
mkdir my_module && cd my_module
nano hello.c

```

Paste this inside, then save (**Ctrl+O**, **Enter**, **Ctrl+X**):

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Hello World Kernel Module");

static int __init hello_init(void) {
    printk(KERN_INFO "Hello Kernel: Module loaded successfully!\n");
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "Goodbye Kernel: Module removed!\n");
}

module_init(hello_init);
module_exit(hello_exit);

```

### 3. Create the Makefile

Open the file:

```bash
nano Makefile

```

Paste this inside (Make sure the spaces before `make` are a single **Tab** key, not spaces!):

```makefile
obj-m += hello.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

```

### 4. Compile and Test

Run these one by one:

```bash
make
sudo insmod hello.ko
dmesg | tail -n 5
sudo rmmod hello
dmesg | tail -n 5

```


This is absolutely perfect! You have successfully compiled your very first Linux kernel module.

Even though there is a wall of text with a few "warnings," this is exactly what a successful build looks like. Here is a quick breakdown of what you are seeing in that output:

* **The Success:** The most important lines are `CC [M] hello.o` and `LD [M] hello.ko`. This tells you that the C Compiler (CC) successfully read your code, and the Linker (LD) successfully built the `hello.ko` (Kernel Object) file. That `.ko` file is your actual compiled plugin!
* **The Warnings:** You can completely ignore the warnings about the "compiler differing," "pahole," and "BTF generation." These are standard, harmless warnings that almost always appear when building an external module on Ubuntu because you don't have the massive 20GB+ kernel debug symbols installed (which you don't need anyway).

### Next Step: Load It Into the Kernel!

Now that you have your `hello.ko` file, let's inject it into your running operating system. Run these commands one by one in your terminal:

1. **Insert the module:**
```bash
sudo insmod hello.ko

```


*(Note: This command won't output anything if it succeeds, it will just return you to the prompt).*
2. **Check the kernel log to see your message:**
```bash
dmesg | tail -n 5

```


3. **Remove the module:**
```bash
sudo rmmod hello

```


4. **Check the kernel log again to see the goodbye message:**
```bash
dmesg | tail -n 5

```

### 🎉 Requirement 1 Complete!

You did it! That last line right there—`Hello Kernel: Module loaded successfully`—means you have successfully injected your own code into a running operating system. You have officially completed the first requirement of your assignment.

If you run `sudo rmmod hello` and check the logs one more time, you will see your goodbye message.

### Decoding the Log Messages

Those messages right above your success line sound a bit scary, but they are completely normal. Here is exactly what the kernel is telling you:

* **The AppArmor lines:** This is just normal background noise from Ubuntu's security system blocking some unrelated background process. You can ignore it.
* **`loading out-of-tree module taints kernel`:** "Tainting" sounds bad, but it simply means you loaded a module that was not compiled directly inside the official Linux source code tree by the kernel developers. It is the kernel's way of saying, "A third party added code to me, so if I crash, don't blame the official developers."
* **`signature and/or required key missing`:** Modern kernels check if modules are cryptographically signed by Microsoft or Canonical (Ubuntu) for security. Yours isn't, so it complains, but it still allows it to run because you have `sudo` privileges.

---

### What's Next: Talking to Userspace

Right now, your module just says hello and goes to sleep. Requirement 2 of your assignment states:

> *Datenaustausch mit dem Userspace: Es soll ein Interface angelegt werden...* (Data exchange with userspace: An interface should be created...)

To do this, we need to add a **Character Device** to your module. This will create a virtual file inside the `/dev/` folder. When a user types a command in their terminal to send text to that file, your kernel module will catch it and save it.




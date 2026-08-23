# Linux Kernel Module Project

> Linux has helped to democratize operating systems. The Linux kernel remains a large and complex body of code, however, and would-be kernel hackers need an entry point where they can approach the code without being overwhelmed by complexity. Often, device drivers provide that gateway. - lld3

Since I'm working on an Apple Silicon Mac, I had to figure out a good way to run a native ARM Linux environment. I ended up using UTM to spin up an Ubuntu Server 26.04 virtual machine. I went with the minimal installation (no GUI) to keep things fast and lightweight.

During the initial OS setup, I created the default user and pulled my public SSH keys straight from GitHub. This was a great shortcut because it let me SSH into the VM from my Mac terminal immediately without messing with passwords. Once inside the VM, we generated a brand new SSH key pair and added the public key to my GitHub settings so we could push commits back to this repository.

## Aufgabe 1: Module Basics

The first step was just getting a basic module to compile, load, and unload safely. I wrote a simple C file using the `module_init` and `module_exit` macros. It uses `printk` to drop a status message into the kernel log (`dmesg`) whenever the module is inserted or removed.

## Aufgabe 2: Userspace Communication

Next, I needed a way for regular terminal commands to talk to the kernel module.

* I registered a character device that shows up at `/dev/fritz_module`.
* When I `echo` text into it, the module uses `kmalloc` to allocate dynamic memory and stores the input safely using `copy_from_user`.
* When I `cat` the device, it reads that dynamic memory and sends it back to the terminal using `copy_to_user`.

## Aufgabe 3: Lists, Timers, and Locks

This part required a pretty big refactor. Instead of just holding a single string in memory, I needed to print the stored text to the kernel log word by word, exactly one second apart.

* **Linked List:** I split the incoming text into individual words and stored them as nodes in a standard kernel linked list (`struct list_head`).
* **Workqueue Timer:** I set up a `delayed_work` task using the kernel's `HZ` macro. It wakes up once a second, pops the first word off the list, prints it, frees the memory, and schedules itself to run again.
* **Mutex Lock:** Because the background timer is reading and deleting from the list at the exact same time a user might be echoing new words into it, I wrapped the list operations in a Mutex lock so the kernel doesn't crash from concurrent access.

## Links
[Ubuntu server download.](https://ubuntu.com/download/server/arm)  

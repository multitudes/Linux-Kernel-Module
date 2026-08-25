# Linux Kernel Module Project

> Linux has helped to democratize operating systems. The Linux kernel remains a large and complex body of code, however, and would-be kernel hackers need an entry point where they can approach the code without being overwhelmed by complexity. Often, device drivers provide that gateway. - lld3

Since I'm working on an Apple Silicon Mac, I had to figure out a good way to run a native ARM Linux environment. I ended up using UTM to spin up an Ubuntu Server 26.04 virtual machine. I went with the minimal installation (no GUI) to keep things fast and lightweight.

During the initial OS setup, I created the default user and pulled my public SSH keys straight from GitHub. This was a great shortcut because it let me SSH into the VM from my Mac terminal immediately without messing with passwords. Once inside the VM, we generated a brand new SSH key pair and added the public key to my GitHub settings so we could push commits back to this repository.

## Development Notes

**Kernel Headers vs. Full Compilation**
If you're referencing older material like the classic *Linux Device Drivers (LDD3)* book, you might see instructions telling you to download a mainline kernel from kernel.org and compile it from scratch. You can completely skip that step. Modern distributions like Ubuntu already provide the necessary development files via the `linux-headers` package. This module is designed to build directly against your existing host kernel, saving you from having to recompile the entire operating system just to test a simple driver.

**Why MODULE_LICENSE("GPL") matters**
You'll notice the `MODULE_LICENSE("GPL")` macro at the bottom of the source code. This isn't just boilerplate or a legal formality—it has actual technical consequences. If you leave this out or use a proprietary license, the kernel will throw a "tainted kernel" warning into `dmesg` as soon as you load the module. More importantly, the kernel physically blocks non-GPL modules from calling certain restricted internal APIs (functions marked with `EXPORT_SYMBOL_GPL`). Tagging the module as GPL keeps the kernel happy and ensures we have full access to everything we need.

**No Standard C Library (libc)**
If you're used to standard C programming, you might notice the total absence of familiar includes like `<stdio.h>` or `<stdlib.h>`. 

Because the kernel operates in an isolated environment (kernel space), it cannot link to standard user-space libraries like `glibc` since those user-space libraries literally rely on the kernel to function. The kernel has to be 100% self-sufficient. 

Because it has zero access to external libraries, all header files must be pulled directly from the Linux kernel's own source tree (which is why they are prefixed with `linux/`, like `<linux/fs.h>`). The kernel developers had to write their own internal versions of standard tools from scratch. For example:

* **Memory:** Instead of `<stdlib.h>` for `malloc()`, we use `<linux/slab.h>` for `kmalloc()`.
* **Printing:** Instead of `<stdio.h>` for `printf()`, we use `<linux/kernel.h>` for `printk()`.
* **Strings:** Instead of the standard `<string.h>`, we use the kernel's internal `<linux/string.h>` to access secure string functions like `strsep()` and `snprintf()`.

**The Kernel Build System (kbuild)**
If you are familiar with traditional C Makefiles, the Makefile for this module might look strangely empty. The core of it boils down to just a single line: `obj-m := fritz_module.o`. 

This works because we aren't writing a standalone Makefile from scratch. Instead, we are hooking directly into the Linux kernel's massive, pre-existing build system (kbuild). By assigning our object file to `obj-m`, we are simply handing our code over and telling kbuild: *"Take this object file and turn it into a loadable kernel module (.ko)."* kbuild automatically handles all the complex linking, architecture-specific compiler flags, and header paths completely behind the scenes. 

If you ever need to build a single module out of multiple C source files, you just specify the final module name and list the ingredients like this:
```makefile
obj-m := final_module.o
final_module-objs := main.o utils.o memory.o
```

**Executing the Build Command**
To actually trigger the build, the command is a bit more complex than a standard user-space build. It typically looks like this (often wrapped inside a `all:` rule in your Makefile for convenience):

`make -C /lib/modules/$(uname -r)/build M=$(PWD) modules`

Here is exactly what this command is doing under the hood:
* **`-C <path>`**: This tells `make` to temporarily change its directory to the Linux kernel headers (or kernel source tree). This is necessary because it needs to read the kernel's massive top-level makefile to know *how* to build a module.
* **`M=$(PWD)`**: Once `make` has read the kernel's rules, this argument tells it to jump right back to your current working directory (where your module's source code and local Makefile live).
* **`modules`**: This is the specific build target. It tells the kernel build system to look at your `obj-m` assignment and compile your code into a loadable `.ko` file.

**The /proc Filesystem (procfs)**
The `/proc` directory is a virtual filesystem created entirely in RAM by the Linux kernel; none of the files inside it actually exist on your hard drive. Instead, reading a file (like `/proc/cpuinfo` or `/proc/modules`) triggers a kernel function that dynamically generates text detailing the system's current internal state. While device files in `/dev` (like `/dev/fritz_module`) are used for active input/output operations with hardware or drivers, `/proc` is strictly used for exposing system information and tuning kernel configurations on the fly. In fact, standard user-space commands rely heavily on this illusion—for example, the `lsmod` command simply reads the virtual `/proc/modules` file to list what is currently loaded in memory.

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
[kernel.org](https://kernel.org)  

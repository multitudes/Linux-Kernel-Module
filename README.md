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

## Device Registration: Raw Char Drivers vs. Misc Framework

When building a Linux kernel module that users can interact with, the kernel needs a way to route user-space operations (like `cat` or `echo`) to your specific driver code. It does this using **Major** (driver category) and **Minor** (specific device) numbers. 

If you reference older documentation like *Linux Device Drivers (LDD3)*, you will see device registration handled very differently than in this project. 

### The "Hard Way" (Raw Character Drivers)
Writing a raw character driver from scratch requires significant manual setup and user-space scripting:
* You must ask the kernel for a new, dynamically allocated Major number using `alloc_chrdev_region()`.
* The kernel does *not* automatically create the endpoint in your `/dev` directory when the module loads. 
* To actually use the driver, you have to write a custom Bash script that loads the `.ko` file, searches through the virtual `/proc/devices` file to find out which Major number the kernel randomly handed you, and then uses the `mknod` command to physically create the `/dev/your_device` file so users can interact with it.

### The "Misc Way" (The `miscdevice` Shortcut)
To save developers from writing setup scripts for simple devices, the kernel provides the `miscdevice` framework, which this project uses. 
* Instead of requesting a unique Major number, our driver piggybacks on the kernel's built-in "misc" subsystem, which statically owns Major number 10.
* By setting `.minor = MISC_DYNAMIC_MINOR` in our setup struct, the kernel safely auto-assigns us an available Minor number without risking conflicts.
* The biggest advantage is that `misc_register()` automatically signals the OS device manager. The exact millisecond the module loads, the `/dev/fritz_module` file is automatically generated for us, and it seamlessly deletes itself when the module is removed. No manual scripts required.

### Concurrency: Mutexes vs. Semaphores

If you are reading older kernel documentation (like the LDD3 book), you will frequently see semaphores and functions like `down_interruptible()` used to manage driver concurrency. While semaphores used to be the standard tool for this job, they are now considered outdated for basic locking scenarios. Modern Linux kernel development strongly prefers the **mutex** (Mutual Exclusion) API. Mutexes are specifically designed for "one-key" binary locking; they are leaner, execute faster, and include strict built-in debugging checks that semaphores lack. In this module, we use a mutex to protect our shared linked list from race conditions. Furthermore, by using `mutex_lock_interruptible()` in our read and write operations, we ensure that if a user process is waiting for the lock, it can still be safely canceled (e.g., via `Ctrl+C`), returning `-ERESTARTSYS` to gracefully back out of the system call.

## Handling Concurrency and Interleaved I/O
While it might seem tempting to over-engineer the driver by isolating user inputs (e.g., mapping user IDs to individual linked lists via hash tables), this module purposefully implements a single, globally shared queue. This design choice strictly follows the philosophy of standard Linux character devices. Because character devices act as raw data streams rather than structured files, the kernel relies on user-space applications to coordinate their own synchronization. If multiple users write to shared endpoints like `/dev/kmsg` or `/dev/null` simultaneously, any resulting data interleaving is inherently considered the users' fault, not the driver's! 

Because of this stream-based architecture, this module enforces a strict 1024-byte limit per `write` system call, rejecting excessively large payloads with `-EINVAL`. This provides a defensive safeguard for kernel memory without introducing the severe complexities of trying to schedule a single background timer across multiple, dynamically changing user queues.
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

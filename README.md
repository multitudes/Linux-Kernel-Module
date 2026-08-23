![linux-kernel-module-header.jpg](linux-kernel-module-header.jpg)

Since I'm working on an Apple Silicon Mac, I had to figure out a good way to run a native ARM Linux environment. I ended up using UTM to spin up an Ubuntu Server 26.04 virtual machine. I went with the minimal installation (no GUI) to keep things fast and lightweight.

During the initial OS setup, I created the default user and pulled my public SSH keys straight from GitHub. This was a great shortcut because it let me SSH into the VM from my Mac terminal immediately without messing with passwords. Once inside the VM, we generated a brand new SSH key pair and added the public key to my GitHub settings so we could push commits back to this repository.

Taking notes is a fantastic idea! Taking a step back to understand the "why" behind the code is exactly how you transition from just copying/pasting to actually thinking like an engineer.

Let's break this down from the absolute beginning so you have a rock-solid understanding of what you just built.

Here are the notes rewritten into natural, flowing paragraphs that sound like you wrote them yourself. You can copy and paste this directly into your notes:

## What is a Linux Kernel Module?

A Linux Kernel Module (LKM) is a piece of compiled code that we can inject directly into that running brain without having to restart the machine. In the old days, if you wanted to add support for a new piece of hardware, you had to modify the base kernel code, recompile the entire massive operating system, and reboot. Modules let us bypass all that and load code dynamically on the fly.

This setup keeps the main kernel super lightweight and efficient. Instead of forcing the kernel to hold the instructions for every single device ever manufactured, it only loads the modules it actually needs for the specific hardware it's running on. It also makes patching things way easier. If there is a bug in something like a Wi-Fi driver, we can just unload that specific module, load in the updated code, and keep the server running without any downtime.

It is a lot like adding an extension to a web browser. You don't have to reinstall your entire browser just to add an ad-blocker; you just install the plugin, it hooks deeply into the browser's core, and you can turn it on or off whenever you want.

### The Makefile

```make
obj-m += hello.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```

The `obj-m` part stands for "object module," and appending our file to it tells the kernel that we want to build a loadable kernel module rather than something permanently built into the kernel itself. Even though our source code is in `hello.c`, we tell it to output `hello.o` because that is the intermediate object file the compiler creates right before it generates the final `hello.ko` plugin file.

The `all` block is what runs by default when we type `make` in the terminal. We use `make -C` to temporarily switch directories over to the official Linux headers installed on the system. 

The `-C` flag simply stands for "change directory." When we use `make -C`, we are telling the make command to temporarily leave our current folder, jump into a completely different directory, and use the Makefile it finds over there instead.

In the context of our kernel module, we don't actually know how to compile kernel code ourselves—it requires hundreds of specific flags and settings. By using `make -C /lib/modules/...`, we are telling our terminal to hop over to the official Linux kernel directory and borrow the massive, complex Makefile that the kernel developers already wrote. It basically hands the hard work off to the kernel's own build system, and once it finishes compiling our code, it hops right back to our folder.

By using `$(shell uname -r)`, the command automatically grabs the exact version of the kernel we are currently running, ensuring it builds against the right files.

The `M=$(PWD)` piece tells it to look back at our current working directory to actually find and compile our code into modules. by setting M equal to our current path, we are telling the kernel's build system, "Hey, the external module you need to compile is located back over here in this specific folder." That way, it knows exactly where to look for our hello.c file and where to spit out the finished .ko file when it is done.

By adding the word modules at the very end, we are giving it a precise command. We are saying, "Don't build the whole kernel right now; just build the external loadable modules".

Finally, the `clean` block at the bottom does the exact same directory-switching trick. But instead of telling the kernel to build our module, it tells it to sweep through our folder and delete all the temporary compiled files and `.ko` files it created earlier. This gives us a completely clean slate whenever we type `make clean`.

## The Macros

Including <linux/module.h> just gives us the ability to use those macros. It is basically like importing a blank form; we still have to actually fill it out and tell the kernel what our specific module is doing.
The MODULE_LICENSE("GPL") line is actually strictly required. The Linux kernel is very protective of its open-source status. If we don't explicitly declare that our code is GPL-compatible, the kernel will complain about being "tainted" by potentially proprietary code.

```c

static LIST_HEAD(word_list);
static DEFINE_MUTEX(word_mutex);

struct word_node {
  char *word;
  struct list_head list;
};
```

The kernel’s linked list implementation (via `<linux/list.h>`) is a circular, doubly-linked list, and it relies heavily on pre-defined macros to manage memory safely. The foundation is `LIST_HEAD()`, which statically initializes the main list head by pointing its `next` and `prev` pointers at itself. When we process user input, we dynamically allocate our custom struct and use `list_add_tail()`. This function takes the `list_head` embedded inside our new struct and wires it into the end of the chain, updating the previous tail and the main head pointers automatically.

The most critical macros in this API handle the extraction of data, specifically `list_first_entry()`. Because the Linux kernel embeds the linked list nodes *inside* the data structures rather than wrapping the data inside a node, you can't just dereference a pointer to get your payload. `list_first_entry()` uses the kernel's famous `container_of()` macro under the hood to perform raw pointer arithmetic—it calculates the exact memory offset from the embedded `list_head` back to the start of your custom struct so you can actually read the string. In our workqueue, we combine this with `list_empty()` to ensure we don't cause a null pointer dereference, and `list_del()` to safely sever the pointers before we extract the word and free the memory.

## static struct delayed_work print_work;

This line is basically us declaring the object that will manage our background threading.

**The `static` Keyword**
We already know this from standard C, but the `static` keyword here is just restricting the scope of the variable to this specific file. 

**`struct delayed_work`**
This is a specific data structure provided by `<linux/workqueue.h>`. You can think of it as a specialized task container. The kernel uses workqueues to push non-urgent tasks into the background so they don't block the main system processes. A standard `work_struct` would tell the kernel to run the task immediately, but a `delayed_work` struct includes a built-in timer. It holds the pointer to the function we want to execute and keeps track of the countdown.

We declare it globally at the top of the file because multiple different functions need to talk to it. Our `module_init` function needs to configure it and kick it off for the very first time, and our actual printing function needs to reference it so it can reschedule itself to run again.

## Aufgabe 1: Module Basics

The first step was just getting a basic module to compile, load, and unload safely. I wrote a simple C file using the `module_init` and `module_exit` macros. It uses `printk` to drop a status message into the kernel log (`dmesg`) whenever the module is inserted or removed.

### The Init Function (`my_init`)

The `__init` macro tells the kernel that this function is only used once during the module loading phase. Once the module is loaded, the kernel drops this function from RAM to save memory. Inside, we use `INIT_DELAYED_WORK()` to map the `print_work` struct we declared earlier to the actual `print_word_work` function containing our logic. Then, `misc_register()` tells the kernel to create the character device endpoint in userspace (`/dev/fritz_module`), which allows standard terminal commands like `echo` and `cat` to trigger the module's read and write routines. Finally, `printk` writes our success message to the `dmesg` log.

### The Exit Function (`my_exit`)

The `__exit` macro marks the function responsible for cleaning up right before the module is removed (via `rmmod`). Teardown order is critical here to prevent kernel panics. First, `misc_deregister()` removes the `/dev/fritz_module` file so userspace can no longer interact with it. Next, `cancel_delayed_work_sync()` ensures our background timer is stopped and fully finished executing before we pull the module out of memory. If a delayed workqueue fired *after* the module was unloaded, the kernel would try to execute code that no longer exists, resulting in an immediate system crash.

### Memory Cleanup

After stopping the background task, we must prevent memory leaks by freeing any dynamically allocated memory left in the linked list. We lock the mutex and iterate through the list using `list_for_each_entry_safe()`. We strictly have to use the `_safe` variant of the macro here rather than a standard iterator because we are actively deleting nodes while traversing them. The `_safe` macro uses a temporary pointer (`tmp`) to hold the next node's address before we call `list_del()` and `kfree()` on the current one. This guarantees we don't accidentally follow a freed, invalid pointer into kernel space.

Since kernel space doesn't have a standard output (`stdout`) attached to a terminal like a normal C program, you can't just use `printf()` to output text. If you try, the compiler will throw an error because the kernel has no concept of a terminal screen.

Instead, the kernel has its own internal, fixed-size memory block dedicated exclusively to logging, known as the **ring buffer**.

When we use `printk()` in our module, we are writing our strings directly into that internal ring buffer. The `dmesg` command (short for "diagnostic messages") is simply the userspace tool we run in the terminal to read that memory buffer and dump its contents to the screen. It is essentially our only window into what the kernel—and our module—is actively doing behind the scenes.

## ring buffer

Because the kernel operates in an environment where it absolutely cannot afford to run out of memory or crash, it pre-allocates a fixed chunk of RAM for the log buffer during boot. Once that chunk fills up, the pointer just wraps right back around to the beginning, and the newest messages start silently overwriting the oldest ones.

This design guarantees that the kernel will never accidentally consume all your system RAM just because a buggy driver is spamming the log with millions of errors. It trades long-term history for absolute system stability.

## module_init

`module_init()` and `module_exit()` are special kernel macros that act like registration desks. When you compile the code, they leave a little tag in the final `.ko` file. Later, when you run `sudo insmod hello.ko` in the terminal, the kernel looks for the `module_init` tag, sees that you attached `my_init` to it, and executes that specific function. When you run `sudo rmmod hello`, it looks for the `module_exit` tag and runs your teardown sequence.

Kernel developers universally put them at the absolute bottom of the file by convention.

## Aufgabe 2: Userspace Communication

The `my_write` function is what gets triggered the second you run a command like `echo "hello world" > /dev/fritz_module` in your terminal. The biggest hurdle here is that user space (your terminal) and kernel space (our module) are strictly isolated in RAM for security reasons. The kernel cannot simply dereference a pointer from user space. If it tried, it would likely trigger a fatal page fault.

To bridge this gap safely, we first have to allocate a fresh block of memory inside the kernel using `kmalloc()`. We pass it the `GFP_KERNEL` flag, which tells the kernel's memory allocator that it is allowed to put the current process to sleep and wait if the system is currently low on free memory.

Once we have that secure kernel memory block, we use the `copy_from_user()` macro. This is a highly optimized, security-checked function that explicitly copies the payload from the restricted `__user` buffer into our newly allocated kernel memory. If it fails—usually because the user space program handed us an invalid memory address—it returns the number of bytes it failed to copy, allowing us to throw an error and abort gracefully rather than crashing the system.

After the string is safely copied into kernel space, we clean it up by swapping the trailing newline character for a null terminator. Since the assignment required word-by-word printing, we use a `while` loop combined with `strsep()` to chop that single string into individual words based on spaces.

For each word we find, we dynamically allocate a new `word_node` struct. We then lock the `word_mutex` to ensure nothing else is touching the list, use `list_add_tail()` to snap our new word onto the end of the chain, and instantly unlock the mutex. Finally, we call `schedule_delayed_work()` to guarantee our background timer is ticking, telling it to wake up in exactly one second (using the `HZ` macro) to start popping words off the front of the list.

```c
static ssize_t dev_write(struct file *filep, const char __user *buffer,
                            size_t len, loff_t *offset) {
char *input_buf, *str_ptr, *token;

if (len > 1024)
return -EINVAL; 
[...]
}
```

The parameters in `dev_write` are handed to us directly by the kernel whenever someone tries to write to our device file. We get a pointer to the file itself, the length of the incoming data (`len`), and the current file offset. But the most interesting part is the `const char __user *buffer`. That `__user` tag doesn't actually change the compiled code, but it tells the kernel's static analysis tools that this memory address belongs to user space. It serves as a strict reminder to developers that this pointer is completely untrusted and we are absolutely not allowed to dereference it directly without using `copy_from_user` first.

Inside the function, we set up a few char pointers that we will use for our string parsing later, and then immediately hit the `if (len > 1024)` check. This is a basic but critical security mechanism. Kernel memory is precious and, unlike standard user applications, it cannot be swapped out to the hard drive if the system gets low on RAM.

If we didn't cap the size, a user could accidentally (or maliciously) pipe a massive 10GB file directly into our device. Our module would blindly pass that massive length to `kmalloc`, attempt to lock up a massive chunk of physical RAM, and likely trigger an Out-Of-Memory kernel panic that would crash the whole VM. If the user tries to send more than our 1024-byte limit, we just reject it immediately and return `-EINVAL`, which is the standard Linux error code for "Invalid Argument". It safely catches the mistake and kicks the error back up to the terminal.

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

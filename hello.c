#include <linux/fs.h> // File system basics, needed since our device acts like a file to the system
#include <linux/init.h> // Needed for the __init and __exit tags so the kernel knows where we start and stop
#include <linux/jiffies.h> // Hooks into the kernel's internal clock and gives us the HZ variable for our 1-second timer
#include <linux/kernel.h> // Brings in printk() so we can actually write to the dmesg log
#include <linux/list.h> // Gives us the built-in kernel linked list structures to store our words
#include <linux/miscdevice.h> // Gives us the shortcuts to easily create our /dev/fritz_module device
#include <linux/module.h> // The main module stuff, gives us MODULE_LICENSE and module_init()
#include <linux/mutex.h> // Provides the locks so our background timer and user inputs don't crash each other
#include <linux/slab.h> // The kernel's version of memory management (kmalloc and kfree for dynamic memory)
#include <linux/string.h> // Standard string tools, mainly so we can use strsep to chop our sentence into words
#include <linux/uaccess.h> // Super important: lets us safely move data between user space and the kernel (copy_to_user)
#include <linux/workqueue.h> // Lets us set up that delayed background task that wakes up every second

MODULE_LICENSE("GPL");          // needed
MODULE_AUTHOR("Laurent Brusa"); // optional, but it is just good practice
MODULE_DESCRIPTION(
    "Aufgabe 3: Benutzung der Kernel API"); // optional, but it is just good
                                            // practice

// Die Liste fuer Aufgabe 3
struct word_node {
  char *word;
  struct list_head list;
};

// Aufgabe 3: Initialisierung
static LIST_HEAD(word_list);
static DEFINE_MUTEX(word_mutex);
static struct delayed_work print_work;

// Aufgabe 3.1. Die Text-Daten aus den internen Speicher
// sollen regelmässig, 1 Wort pro Sekunde, in das
// Kernel Log geschrieben werden
static void print_word_work(struct work_struct *work) {
  struct word_node *node = NULL;

  mutex_lock(&word_mutex);
  if (!list_empty(&word_list)) {
    node = list_first_entry(&word_list, struct word_node, list);
    list_del(&node->list);
  }
  mutex_unlock(&word_mutex);

  if (node) {
    printk(KERN_INFO "fritz_module word: %s\n", node->word);
    kfree(node->word);
    kfree(node);

    schedule_delayed_work(&print_work, HZ);
  }
}

// Aufgabe 2.2: Text-Daten aus dem internen Speicher an den Userspace
// zurückgeben
static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len,
                        loff_t *offset) {
  char *temp_buf;
  struct word_node *node;
  size_t pos = 0;

  if (*offset > 0)
    return 0;

  temp_buf = kzalloc(1024, GFP_KERNEL);
  if (!temp_buf)
    return -ENOMEM;

  mutex_lock(&word_mutex);
  list_for_each_entry(node, &word_list, list) {
    pos += snprintf(temp_buf + pos, 1024 - pos, "%s ", node->word);
    if (pos >= 1023)
      break;
  }
  mutex_unlock(&word_mutex);

  if (pos > 0)
    temp_buf[pos - 1] = '\n';
  else
    pos = snprintf(temp_buf, 1024, "List is empty\n");
  if (len < pos)
    pos = len;

  if (copy_to_user(buffer, temp_buf, pos)) {
    kfree(temp_buf);
    return -EFAULT;
  }

  kfree(temp_buf);
  *offset += pos;
  return pos;
}

// Aufgabe 2.1:
// the kernel will pass the args to us
// __user is a tag to indicate the memory address belongs to user space
static ssize_t dev_write(struct file *filep, const char __user *buffer,
                         size_t len, loff_t *offset) {
  char *input_buf, *str_ptr, *token;

  if (len > 1024)
    return -EINVAL; // error invalid argument

  input_buf = kzalloc(len + 1, GFP_KERNEL);
  if (!input_buf)
    return -ENOMEM; // error no memory
  if (copy_from_user(input_buf, buffer, len)) {
    kfree(input_buf);
    return -EFAULT;
  }

  str_ptr = input_buf;
  while ((token = strsep(&str_ptr, " \n\t")) != NULL) {
    if (*token == '\0')
      continue;

    struct word_node *new_node = kmalloc(sizeof(struct word_node), GFP_KERNEL);
    if (new_node) {
      new_node->word = kstrdup(token, GFP_KERNEL);

      mutex_lock(&word_mutex);
      list_add_tail(&new_node->list, &word_list);
      mutex_unlock(&word_mutex);
    }
  }
  kfree(input_buf);

  schedule_delayed_work(&print_work, HZ);

  return len;
}

// this intercepts the write and read system calls to my module and tell
// the system to use the dev_read and dev_write instead.
// example when I do echo "hello" > /dev/fritz_module the dev_write is called
static struct file_operations fops = {.read = dev_read, .write = dev_write};

static struct miscdevice my_misc_device = {
    .minor = MISC_DYNAMIC_MINOR, .name = "fritz_module", .fops = &fops};

// a macro which will call my my_init function at the start
static int __init my_init(void) {
  INIT_DELAYED_WORK(&print_work, print_word_work);
  misc_register(&my_misc_device);
  printk(KERN_INFO "Hello Kernel: Module loaded successfully.\n");
  return 0;
}

// __exit is another macro called when it is time to tear down
static void __exit my_exit(void) {
  struct word_node *node, *tmp;

  misc_deregister(&my_misc_device);
  cancel_delayed_work_sync(&print_work);

  // free the memory from the linked list using a mutex
  mutex_lock(&word_mutex);
  list_for_each_entry_safe(node, tmp, &word_list, list) {
    list_del(&node->list);
    kfree(node->word);
    kfree(node);
  }
  mutex_unlock(&word_mutex);

  // prints to the ring buffer
  printk(KERN_INFO "Goodbye Kernel: Module removed.\n");
}

module_init(my_init);
module_exit(my_exit);

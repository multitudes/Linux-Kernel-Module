// just about all module code has the following:
#include <linux/module.h>
#include <linux/init.h> 

// and the rest
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Laurent Brusa");
MODULE_DESCRIPTION("Aufgabe fuer Bewerbende");

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
static ssize_t dev_write(struct file *filep, const char __user *buffer,
                         size_t len, loff_t *offset) {
  char *input_buf, *str_ptr, *token;

  if (len > 1024)
    return -EINVAL;

  input_buf = kzalloc(len + 1, GFP_KERNEL);
  if (!input_buf)
    return -ENOMEM;
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

static struct file_operations fops = {
    .owner = THIS_MODULE, .read = dev_read, .write = dev_write};
static struct miscdevice my_misc_device = {
    .minor = MISC_DYNAMIC_MINOR, .name = "fritz_module", .fops = &fops};

static int __init my_init(void) {
  INIT_DELAYED_WORK(&print_work, print_word_work);
  misc_register(&my_misc_device);
  printk(KERN_ALERT "Hello, world\n");
  return 0;
}

static void __exit my_exit(void) {
  struct word_node *node, *tmp;

  misc_deregister(&my_misc_device);
  cancel_delayed_work_sync(&print_work);

  mutex_lock(&word_mutex);
  list_for_each_entry_safe(node, tmp, &word_list, list) {
    list_del(&node->list);
    kfree(node->word);
    kfree(node);
  }
  mutex_unlock(&word_mutex);

  printk(KERN_ALERT "Goodbye, cruel world\n\n");
}

// The use of module_initis mandatory
module_init(my_init);

module_exit(my_exit);

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <asm/uaccess.h>          // Required for the copy to user function
#include <net/sock.h>
#include <linux/netlink.h>
#include <net/netlink.h>
#include <linux/skbuff.h>
#include <net/net_namespace.h>

#define  DEVICE_NAME "ebbchar"    ///< The device will appear at /dev/ebbchar using this value
#define  CLASS_NAME  "ebb"        ///< The device class -- this is a character device driver

#define NETLINK_USER 31
#define MYPROTO NETLINK_USERSOCK
#define MAXMASS  256


MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Derek Molloy");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple Linux char driver for the BBB");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static struct class*  ebbcharClass  = NULL; ///< The device-driver class struct pointer
static struct device* ebbcharDevice = NULL; ///< The device-driver device struct pointer
struct sock *nl_sk = NULL;
struct sock *nl_sk1 = NULL;

// The prototype functions for the character driver -- must come before the struct definition
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

//netlink decliertion
static void hello_nl_recv_msg(struct sk_buff *skb);
static int __init hello_init(void);
static void __exit hello_exit(void);
static void send_to_user(void);

static struct file_operations fops =
{
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

static int __init ebbchar_init(void){
   hello_init();
   printk(KERN_INFO "EBBChar: Initializing the EBBChar LKM\n");

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "EBBChar failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "EBBChar: registered correctly with major number %d\n", majorNumber);

   // Register the device class
   ebbcharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(ebbcharClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(ebbcharClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "EBBChar: device class registered correctly\n");

   // Register the device driver
   ebbcharDevice = device_create(ebbcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(ebbcharDevice)){               // Clean up if there is an error
      class_destroy(ebbcharClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(ebbcharDevice);
   }
   printk(KERN_INFO "EBBChar: device class created correctly\n"); // Made it! device was initialized
   return 0;
}

static void __exit ebbchar_exit(void){
   device_destroy(ebbcharClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(ebbcharClass);                          // unregister the device class
   class_destroy(ebbcharClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   printk(KERN_INFO "EBBChar: Goodbye from the LKM!\n");
   hello_exit();
}


static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);

   if (error_count==0){            // if true then have success
      printk(KERN_INFO "EBBChar: Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "EBBChar: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
   size_of_message = strlen(message);                 // store the length of the stored message
   printk(KERN_INFO "EBBChar: Received %zu characters from the user\n", len);
   send_to_user();

   return len;
}

static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "EBBChar: Device successfully closed\n");
   return 0;
}

//netlink func
static int __init hello_init(void) {


    //pf_netlink
    printk("Entering: %s\n",__FUNCTION__);
    //This is for 3.6 kernels and above.
  struct netlink_kernel_cfg cfg = {
    .input = hello_nl_recv_msg,
  };

  nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
  //nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, 0, hello_nl_recv_msg,NULL,THIS_MODULE);
  if(!nl_sk)
  {

    printk(KERN_ALERT "Error creating socket.\n");
    return -10;

  }

    //af netlink
    pr_info("Inserting hello module.\n");

    nl_sk1 = netlink_kernel_create(&init_net, MYPROTO, NULL);
    if (!nl_sk1) {
        pr_err("Error creating socket.\n");
        return -10;
    }

    return 0;
}

static void send_to_user(void)
{
    struct sk_buff *skb1;
    struct nlmsghdr *nlh1;
    int msg_size1 = strlen(message) + 1;
    int res1;

    pr_info("Creating skb.\n");
    skb1 = nlmsg_new(NLMSG_ALIGN(msg_size1 + 1), GFP_KERNEL);
    if (!skb1) {
        pr_err("Allocation failure.\n");
        return;
    }

    nlh1 = nlmsg_put(skb1, 0, 1, NLMSG_DONE, msg_size1 + 1, 0);
    strcpy(nlmsg_data(nlh1), message);

    pr_info("Sending skb.\n");
    res1 = nlmsg_multicast(nl_sk1, skb1, 0, NETLINK_USER, GFP_KERNEL);
    if (res1 < 0)
        pr_info("nlmsg_multicast() error: %d\n", res1);
    else
        pr_info("Success.\n");
}

static void hello_nl_recv_msg(struct sk_buff *skb) {

struct nlmsghdr *nlh;
int pid;
struct sk_buff *skb_out;
int msg_size;

printk(KERN_INFO "Entering: %s\n", __FUNCTION__);

msg_size = MAXMASS;

nlh=(struct nlmsghdr*)skb->data;
printk(KERN_INFO "Netlink received msg payload:%s\n",(char*)nlmsg_data(nlh));
pid = nlh->nlmsg_pid; /*pid of sending process */

skb_out = nlmsg_new(msg_size,0);

if(!skb_out)
{

    printk(KERN_ERR "Failed to allocate new skb\n");
    return;

}
strcpy(message,(char*)nlmsg_data(nlh));
printk(KERN_INFO "Netlink received mmmmmmmmmm:%s\n",message);

}



static void __exit hello_exit(void) {
    printk(KERN_INFO "exiting hello module\n");
    netlink_kernel_release(nl_sk);
    netlink_kernel_release(nl_sk1);
}

module_init(ebbchar_init);
module_exit(ebbchar_exit);

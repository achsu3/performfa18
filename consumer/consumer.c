#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMANDA");
MODULE_DESCRIPTION("Simulate consumer/producer problem via queue");
MODULE_VERSION("0.1");




static int _init consumer_init(void){
    printk(KERN_INFO "init consumer module");
    return 0;
    }
static void _exit consumer_exit(void){
    printk(KERN_INFO "exit consumer module");
    }
//register module functions
module_init(consumer_init);
module_exit(consumer_exit);

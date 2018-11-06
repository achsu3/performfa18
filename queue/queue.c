#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include<linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/syscalls.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMANDA");
MODULE_DESCRIPTION("");
MODULE_VERSION(".01");


int len,temp,i=0,ret;
char *empty="Queue Empty  \n";
int emp_len,flag=1;
//flag - make sure that you're only returning 1 node at a time
//emp_len - 
static struct k_list *node;
struct list_head *head;
struct list_head test_head;
int new_node=1;
char *msg;

//create queue
struct k_list {
  struct list_head queue_list;//create list
  char *data;//hold data
};


//dequeue
ssize_t dequeue(struct file *filp,char *buf,size_t count,loff_t *offp){
  if(list_empty(head)){//make sure the list isn't empry before you try popping
    msg=empty;
    if(flag==1) {//the first time you dequeue and it's empty - return 1
      ret=emp_len;
      flag=0;
    }
    else if(flag==0){//the second time just return 0
      ret=0;
      flag=1;
    }
    temp=copy_to_user(buf,msg, count);//temp holds # bytes that could not be copied
    printk(KERN_INFO "\nStack empty\n");
    return ret;
  }
  if(new_node == 1) {//you have a node to dequeue
    node=list_first_entry(head,struct k_list ,queue_list);
    msg=node->data;
    ret=strlen(msg);//return the data from the node
    new_node=0;
	kfree(msg);
	kfree(node);
  }
  if(count>ret) {
    count=ret;
  }
  ret=ret-count;
  temp=copy_to_user(buf,msg, count);
  printk(KERN_INFO "\n data = %s \n" ,msg);
  if(count==0) {
    list_del(&node->queue_list);//delete the node youre dequeueing
    new_node=1;//there are more nodes to dequeue
  }
  return count;
}

//enqueue
ssize_t enqueue(struct file *filp,const char *buf,size_t count,loff_t *offp){
  msg=kmalloc(10*sizeof(char),GFP_KERNEL);
  temp=copy_from_user(msg,buf,count);
  node=kmalloc(sizeof(struct k_list *),GFP_KERNEL);
  node->data=msg;
  list_add_tail(&node->queue_list,head);
  return count;
}

//file ops for the proc entry
struct file_operations proc_fops = {
  .read = dequeue,
  .write = enqueue,
};

//create new proc
void create_new_proc_entry(void){
  proc_create("queue",0,NULL,&proc_fops);
}
//initialize the queue!
int queue_init (void) {
  create_new_proc_entry();
  head = kmalloc(sizeof(struct list_head *),GFP_KERNEL);
  INIT_LIST_HEAD(head);
  emp_len=strlen(empty);
  printk("init queue");
  return 0;
}
//delete everything
void queue_cleanup(void) {

//free any remaining nodes on the queue



 kfree(head);
 remove_proc_entry("queue",NULL);
 printk("cleanin' queue");

 // add code to free thigns in case they are not empty
}

module_init(queue_init);
module_exit(queue_cleanup);

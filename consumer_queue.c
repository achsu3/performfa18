#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include<linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

int len,temp,i=0,ret;
char *empty="Queue Empty  \n";
int emp_len,flag=1;

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
//file ops for the proc entry
struct file_operations proc_fops = {
  read: dequeue,
  write: enqueue
};

//dequeue
int dequeue(struct file *filp,char *buf,size_t count,loff_t *offp){
  if(list_empty(head)){//make sure the list isn't empry before you try popping
    msg=empty;
    if(flag==1) {
      ret=emp_len;
      flag=0;
    }
    else if(flag==0){
      ret=0;
      flag=1;
    }
    temp=copy_to_user(buf,msg, count);//temp holds # bytes that could not be copied
    printk(KERN_INFO "\nStack empty\n");
    return ret;
  }
  if(new_node == 1) {
    node=list_first_entry(head,struct k_list ,queue_list);
    msg=node->data;
    ret=strlen(msg);
    new_node=0;
  }
  if(count>ret) {
    count=ret;
  }
  ret=ret-count;
  temp=copy_to_user(buf,msg, count);
  printk(KERN_INFO "\n data = %s \n" ,msg);
  if(count==0) {
    list_del(&node->queue_list);
    new_node=1;
  }
  return count;
}

//enqueue
int enqueue(struct file *filp,const char *buf,size_t count,loff_t *offp){
  msg=kmalloc(10*sizeof(char),GFP_KERNEL);
  temp=copy_from_user(msg,buf,count);
  node=kmalloc(sizeof(struct k_list *),GFP_KERNEL);
  node->data=msg;
  list_add_tail(&node->queue_list,head);
  return count;
}

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
  return 0;
}
//delete everything
void queue_cleanup(void) {
 remove_proc_entry("queue",NULL);
}

module_init(queue_init);
module_exit(queue_cleanup);

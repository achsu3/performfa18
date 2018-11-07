#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include<linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
//use this https://people.freedesktop.org/~narmstrong/meson_drm_doc/dev-tools/gdb-kernel-debugging.html 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMANDA");
MODULE_DESCRIPTION("");
MODULE_VERSION(".01");

int _weight = 13;//assigned and incremented for testing purposes
int len,temp,i=0,ret;
char *empty="Queue Empty  \n";
int emp_len,flag=1;
//flag - make sure that you're only returning 1 node at a time
//emp_len -
static struct k_list node;
struct list_head *head;
struct list_head test_head;
int new_node=1;
char *msg;
static struct priority_queue *pqueue;

//create queue struct
struct priority_queue{
  //array of reqs
  int size;//current size
  int max_size;//drop if this is reached
  struct k_list * requests; //array of requeusts in queue
 // unsigned de_i; //index to dequeue from
};
struct k_list {
  char *data;//hold data
  int weight;//
};

void heap_swap(int i, int j){//swap two indexes in the requests array
  //swap each item seperately
  int tempweight;
  char * tempdata = pqueue->requests[i].data;
  pqueue->requests[i].data = pqueue->requests[j].data;
  pqueue->requests[j].data = tempdata;

  tempweight = pqueue->requests[i].weight;
  pqueue->requests[i].weight = pqueue->requests[j].weight;
  pqueue->requests[j].weight = tempweight;
}

void max_heapify(void){
  int i = 1;
  int l,r;
  int largest = 0;
  while(true){
    l = 2i-1;
    r = 2i;
    if((l<=pqueue->size-1) && pqueue->requests[l].weight>pqueue->requests[r].weight){
      largest = l;
    }
    else{
      largest = i-1;
    }
    if((r<=pqueue->size-1)&&pqueue->requests[r].weight>pqueue->requests[l].weight){
      largest = r;
    }
    if(largest == i){
      return;
    }
    heap_swap(i-1,largest);
    i = largest + 1;
  }
}


//dequeue
ssize_t dequeue(struct file *filp,char *buf,size_t count,loff_t *offp){
  int de=0;
  if(pqueue->size == 0){//make sure the list isn't empry before you try popping
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
    printk(KERN_INFO "\nQ empty\n");
    return ret;
  }
  de = 0;
  if(new_node == 1) {//you have a node to dequeue
    node=pqueue->requests[1];
    msg=node.data;
    ret=strlen(msg);//return the data from the node
    new_node=0;
    de = 1;
  if(count>ret) {
    count=ret;
  }
  ret=ret-count;
  temp=copy_to_user(buf,msg, count);
  printk(KERN_INFO "\n data = %s \n" ,msg);
  if(count==0) {
    //list_del(&node->queue_list);//delete the node youre dequeueing
    new_node=1;//there are more nodes to dequeue
  }
  //re-heapify the queue
  //first put the last one in the first one's spot
  heap_swap(0,pqueue->size-1);
  //decrement size of pqueue
  pqueue->size = pqueue->size -1;
  //call the heapify function
  max_heapify();
  //return count;
  }
  return count;
}

//enqueue
ssize_t enqueue(struct file *filp,const char *buf,size_t count,loff_t *offp){
  if(pqueue->size == pqueue->max_size){//if it is full
    return count;
  }
  msg=kmalloc(10*sizeof(char),GFP_KERNEL);
  temp=copy_from_user(msg,buf,count);
  pqueue->requests[pqueue->size].data=msg;
  pqueue->requests[pqueue->size].weight = _weight;
  _weight ++;
  //struct k_list * newnode= //no need to malloc for this again kmalloc(sizeof(struct k_list),GFP_KERNEL);
  //newnode->data=msg;
  //newnode->weight = _weight;//testing purposes
  //pqueue->requests[pqueue->size]=node;

  pqueue->size = pqueue->size +1;
  max_heapify();
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
  pqueue = kmalloc(sizeof(struct priority_queue),GFP_KERNEL);
  pqueue->size = 0;
  pqueue->max_size = 6;
  pqueue->requests = kmalloc_array(6, sizeof(struct k_list),GFP_KERNEL);
  emp_len=strlen(empty);
  printk("init queue");
  return 0;
}
//delete everything
void queue_cleanup(void) {
 kfree(pqueue->requests);
 kfree(pqueue);// - why doesn't this work? 
 remove_proc_entry("queue",NULL);
 printk("cleanin' queue");
}

module_init(queue_init);
module_exit(queue_cleanup);

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

//create queue struct
struct priority_queue{
  //array of reqs
  int size;//current size
  int max_size;//drop if this is reached
  struct k_list * requests; //array of requeusts in queue

}
struct k_list {
  char *data;//hold data
  int weight;//
};

void heap_swap(struct priority_queue * pqueue, int i, int j){//swap two indexes in the requests array
  //swap each item seperately
  char * tempdata = pqueue->requests[i]->data;
  pqueue->requests[i]->data = pqueue->requests[j]->data;
  pqueue->requests[j]->data = tempdata;

  int tempweight = pqueue->requests[i]->weight;
  pqueue->requests[i]->weight = pqueue->requests[j]->weight;
  pqueue->requests[j]->weight = tempweight;
}

void max_heapify(struct priority_queue * pqueue){
  int i = 1;
  int l,r;
  int largest = 0;
  while(true){
    l = 2i-1;
    r = 2i;
    if((l<=pqueue->size-1) && pqueue->requests[l]->weight>pqueue->requests[r]->weight){
      largest = l;
    }
    else{
      largest = i-1;
    }
    if((r<=pqueue->size-1)&&pqueue->requests[r]->weight>pqueue->requests[l]->weight){
      largest = r;
    }
    if(largest == i){
      return;
    }
    heap_swap(pqueue,i-1,largest);
    i = largest + 1;
  }
}


//dequeue
ssize_t dequeue(struct priority_queue * pqueue,struct file *filp,char *buf,size_t count,loff_t *offp){
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
  bool de = false;
  if(new_node == 1) {//you have a node to dequeue
    node=pqueue->requests[1];
    msg=node->data;
    ret=strlen(msg);//return the data from the node
    new_node=0;
    de = true;
  }
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
  if(de){
    //shift everything up 1
    int i = 0;
    while(i<(pqueue->size-2))
      pqueue->requests[i]->data = pqueue->requests[i+1]->data;
      pqueue->requests[i]->weight = pqueue->requests[i+1]->weight;
  }
  //make the last one null
  pqueue->requests[pqueue->size-1]->data = NULL;
  pqueue->requests[pqueue->size-1]->weight = NULL;

  //decrement size of pqueue
  pqueue->size = pqueue->size -1;
  return count;
}

//enqueue
ssize_t enqueue(struct priority_queue* pqueue, struct file *filp,const char *buf,size_t count,loff_t *offp){
  msg=kmalloc(10*sizeof(char),GFP_KERNEL);
  temp=copy_from_user(msg,buf,count);
  node=kmalloc(sizeof(struct k_list *),GFP_KERNEL);
  node->data=msg;
  node->weight = 6;//testing purposes
  if(pqueue->size == pqueue->max_size){//if it is full
    return count;
  }
  pqueue->requests[pqueue->size]=node;
  pqueue->size = pqueue->size +1;
  max_heapify(pqueue);
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
  struct priority_queue pqueue* = kmalloc(sizeof(struct priority_queue *),GFP_KERNEL);
  pqueue->size = 0;
  pqueue->max_size = 6;
  pqueue->requests = kmalloc_array(6, sizeof(struct k_list *),GFP_KERNEL);
  emp_len=strlen(empty);
  printk("init queue");
  return 0;
}
//delete everything
void queue_cleanup(void) {
 remove_proc_entry("queue",NULL);
 kfree(pqueue->requests);
 kfree(pqueue);
 printk("cleanin' queue");
}

module_init(queue_init);
module_exit(queue_cleanup);

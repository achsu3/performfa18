#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include<linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/list_sort.h>
#include <sys/queue.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMANDA");
MODULE_DESCRIPTION("");
MODULE_VERSION(".01");


int len,temp,i=0,ret;
char *empty="Queue Empty  \n";
int emp_len,flag=1;
//int capacity;
//flag - make sure that you're only returning 1 node at a time
//emp_len -
struct k_list *node;
struct k_list * head; //root or head or whatever

int new_node=1;
char *msg;

//tree implementation
struct k_list {
  //struct node head;
  char *data;//hold data
  int weight;//weight by which the list will be sorted
  struct k_list * parent_;
  struct k_list * right_;
  struct k_list * left_;
};

//function to swap two nodes
struct k_list * swap(struct k_list * node_1, struct k_list * node_2){
  //switch parents
  //case: node_1 is parent of node_2
  if(node_1->parent_ == node_2){
    if(node_2->parent_){
      if(node_2->parent_->right_ == node_2){
        node_2->parent_->right_ = node_1;
      }
      else if(node_2->parent_->left_ == node_2){
        node_2->parent_->left_ = node_2;
      }
    }
    node_2->parent_ = node_1;
  }
  //case: node_2 is parent of node_1
  else if(node_2->parent_ == node_1){
    if(node_1->parent_){
      if(node_1->parent_->right_ == node_1){
        node_1->parent_->right_ = node_2;
      }
      else if(node_1->parent_->left_ == node_1){
        node_1->parent_->left_ = node_2;
      }
    }
    node_1->parent_ = node_2;
  }
  //case: neither is parent of eachother:
  else{
    if(node_1->parent_){
      if(node_1->parent_->right_ == node_1){
        node_1->parent_->right_ = node_2;
      }
      else if(node_1->parent_->left_ == node_1){
        node_1->parent_->left_ = node_2;
      }
    }
    if(node_2->parent_){
      if(node_2->parent_->right_ == node_2){
        node_2->parent_->right_ = node_1;
      }
      else if(node_2->parent_->left_ == node_2){
        node_2->parent_->left_ = node_2;
      }
    }
  }
  //switch left
  struct k_list *temp = node_1->left_;
  node_1->left_ = node_2->left_;
  node_2->left_ = temp;

  //switch right
  temp = node_1->right_;
  node_1->right_ = node_2->right_;
  node_2->right_ = temp;

  //return the second node;
  return node_2;
}


//dequeue
ssize_t dequeue(struct file *filp,char *buf,size_t count,loff_t *offp){
  if(!head->right_ && !head->left){//make sure the list isn't empry before you try popping
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
    node=head;//you will be removing the root because it has the highest weight
    msg=node->data;
    ret=strlen(msg);//return the data from the node
    new_node=0;

    //update the tree to be organized without the root
    //if the head is the only thing left, skip this
    if(!head->right&&!head->left){
      goto done;
    }
    //take one of the leaves and replace the root with it, switch until the it is ordered again
    //****should be rightmost leaf
    struct k_list leaf = head;
    //find a leaf
    while(leaf->left || leaf->right){
      if(leaf->left){
        leaf = leaf->left;
      }
      else if(leaf->right){
        leaf = leaf->right;
      }
    }
    //now switch until you ordered again
    head = leaf;
    while(leaf->right_ || leaf->left_){
      //case 1: leaf is bigger than left and right
      if(leaf->right_ && leaf->left_ ){
        if((leaf->weight > leaf->left_->weight)&&(leaf->weight > leaf->right_->weight)){
          goto: done;
        }
        if(leaf->right_->weight > leaf->left_->weight){
          leaf = swap(leaf, leaf->right_);
          continue;
        }
        else{
          leaf = swap(leaf, leaf->left_);
          continue;
        }
      }
      //case 2: left is larger than right
      else if(leaf->right_){
        if(leaf->right_->weight > leaf->weight){
          //switch
          leaf = swap(leaf, leaf->right_);
          continue;
        }
      }
      //case 3: right is bigger than left
      else if(leaf->left_){
        if(leaf->right->weight > leaf->weight){
          //switch
          leaf = swap(leaf, leaf->left_);
          continue;
        }
      }
    }
    done:
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
    //list_del(&node->queue_list);//delete the node youre dequeueing
    new_node=1;//there are more nodes to dequeue
  }
  return count;
}

//enqueue
ssize_t enqueue(struct file *filp,const char *buf,size_t count,loff_t *offp){
  msg=kmalloc(10*sizeof(char),GFP_KERNEL);
  temp=copy_from_user(msg,buf,count);
  node=kmalloc(sizeof(struct k_list *),GFP_KERNEL);
  node->right_ = NULL;
  node->left_ = NULL;
  node->parent_ = NULL;
  node->data=msg;
  node->weight = 6; //for testing i guess
  if(head->weight == -1){
    kfree(node);
    head->data = msg;
    head->weight = 10;
    goto finishenqueue;
  }

  //find where you can add the new node
  //start as a leaf and swap upwards

  //find a leaf **** should be leftmost leaf
  struct k_list * curr = head;
  while(curr->left_ || curr->right_){
    if(curr->left_){
      curr = curr->left_;
    }
    else if(curr->right_){
      curr = curr->right_;
    }
  }
  //now curr is a leaf
  //attach node to it
  node->parent_ = curr;
  curr->left_ = node;

  //swap until curr is less than its parent
  while(curr->weight > curr->parent_->weight){
    curr = swap(curr->parent_, curr);
  }
  finishenqueue:
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
  head = kmalloc(sizeof(struct k_list *),GFP_KERNEL);
  //INIT_LIST_HEAD(head);
  head->parent_ = NULL;
  head->left_ = NULL;
  head->right_=NULL;
  head->data = NULL;
  head->weight = -1;
  emp_len=strlen(empty);
  printk("init queue");
  return 0;
}
//delete everything
void queue_cleanup(void) {
 //create a list of all the nodes in the tree
 //@TODO

 //delete all information in the list
 remove_proc_entry("queue",NULL);
 printk("cleanin' queue");
}

module_init(queue_init);
module_exit(queue_cleanup);

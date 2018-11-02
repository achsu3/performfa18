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

/* @QUESTION: Is there a reason while you need all these to be global?
 * temp, i, ret, and msg all seem local variables to me used within function and
 * their values are not really transferred between different scopes, right?
 *
 * It's okay if it's working in this module since we only want to port the
 * logic, but we might not have this luxury when writing the code deep into the
 * networking stack.
 */
int len,temp,i=0,ret;
char *empty="Queue Empty  \n";
int emp_len,flag=1;
//int capacity;
//flag - make sure that you're only returning 1 node at a time
//emp_len -
struct k_list *node = NULL;
struct k_list * head = NULL; //root or head or whatever

int new_node=1;
char *msg = 0;

/* @Suggestion: One thing you can do here is if you need a lot of metadata about
 * the queue, then it's not uncommon to give it its own structure, something
 * like
 * struct priority_queue {
 *	struct k_list *top;
 *	int size;
 *	... whatever else here that you need about the queue as a whole
 * };
 * and then you only one copy of this that you maintain all around
 */

//will implement in actual kernel

struct priority_queue{
  struct k_list * head;
  struct k_list * min;
  //int size;
}

//tree implementation
struct k_list{
	//struct node head;
	char *data;//hold data
	int weight;//weight by which the list will be sorted
	struct k_list * parent_;
	struct k_list * right_;
	struct k_list * left_;
};

//function to swap two nodes
/* @FIX: This was conflicting with the kernel's own swap macro defined in
 * kernel.h -- renamed to heap_swap and fixed calls.
 */
struct k_list * heap_swap(struct priority_queue * pqueue, struct k_list * node_1, struct k_list * node_2){
	struct k_list *temp;

  //swap any data that is in priority_queue struct
  if(pqueue->head==node_1){
    pqueue->head = node_2;
  }
  if(pqueue->min == node_1){
    pqueue->min = node_2;
  }
  if(pqueue->head==node_2){
    pqueue->head = node_1;
  }
  if(pqueue->min == node_2){
    pqueue->min = node_1;
  }

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
	temp = node_1->left_;
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
ssize_t dequeue(struct priority_queue * pqueue, struct file *filp,char *buf,size_t count,loff_t *offp){
	struct k_list *leaf;

	if(!pqueue->head->right_ && !pqueue->head->left_){//make sure the list isn't empry before you try popping
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
		if(pqueue->min == pqueue->head){
			goto done;
		}
		//take one of the leaves and replace the root with it, switch until the it is ordered again
		//****should be rightmost leaf
		leaf = head;
		//find a leaf
		while(leaf->left_ || leaf->right_){
			if(leaf->left_){
				leaf = leaf->left_;
			}
			else if(leaf->right_){
				leaf = leaf->right_;
			}
		}
		//now switch until you ordered again
		head = leaf;
		while(leaf->right_ || leaf->left_){
			//case 1: leaf is bigger than left and right
			if(leaf->right_ && leaf->left_ ){
				if((leaf->weight > leaf->left_->weight)&&(leaf->weight > leaf->right_->weight)){
					goto done;
				}
				if(leaf->right_->weight > leaf->left_->weight){
					leaf = heap_swap(leaf, leaf->right_);
					continue;
				}
				else{
					leaf = heap_swap(leaf, leaf->left_);
					continue;
				}
			}
			//case 2: left is larger than right
			else if(leaf->right_){
				if(leaf->right_->weight > leaf->weight){
					//switch
					leaf = heap_swap(leaf, leaf->right_);
					continue;
				}
			}
			//case 3: right is bigger than left
			else if(leaf->left_){
				if(leaf->right_->weight > leaf->weight){
					//switch
					leaf = heap_swap(leaf, leaf->left_);
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

	/* @QUESTION: it seems msg is being free'd on line 174, this might
	 * create an issue later on because you're accessing a free'd memory
	 * area. Was is the intention of the free operation after the done
	 * label?
	 */
	temp=copy_to_user(buf,msg, count);
	printk(KERN_INFO "\n data = %s \n" ,msg);
	if(count==0) {
		//list_del(&node->queue_list);//delete the node youre dequeueing
		new_node=1;//there are more nodes to dequeue
	}
	return count;
}

/* @SUGGESTION: You could do something like this to write less repetitive code which
 * will help in reducing bugs
 */
static inline struct k_list *create_empty_node(char *data) {
	struct k_list *tmp;

	tmp = kmalloc(sizeof(struct k_list), GFP_KERNEL);
	if (!tmp) {
		/* this is just fancy error printing, it's a shortcut for printk
		 * with the appropriate flags set
		 */
		pr_err("Could not allocate memory!");
		return 0;
	}

	tmp->left_	= 0;
	tmp->right_	= 0;
	tmp->parent_	= 0;
	tmp->data	= data;

	return tmp;
}

static inline void free_node(struct k_list *node) {
	if (node) {
		/* need to do two layers of freeing */
		if (node->data)
			kfree(node->data);
		node->data = 0; /* for safety, disable access to that memory region */

		kfree(node);
	}
}

//enqueue
ssize_t enqueue(struct priority_queue * pqueue, struct file *filp,const char *buf,size_t count,loff_t *offp){
	struct k_list *curr;
	msg=kmalloc(10*sizeof(char),GFP_KERNEL);
	temp=copy_from_user(msg,buf,count);
	/* @FIX:
	 * this was
	 *	node = kmalloc(sizeof(struct k_list *), GFP_KERNEL)
	 *
	 * However, you actually need a memory area of size = size of the
	 * actual k_list structure and not a pointer to it.
	 * sizeof(struct k_list *) will actually return the size of the pointer
	 * and the size of the structure, which is typically machine
	 * dependent, but commonly it will be 64 bits or 8 bytes.
	 * So what you actually need is sizeof(struct k_list).
	 */
	node=kmalloc(sizeof(struct k_list),GFP_KERNEL);
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

	//take rightmost leaf
	curr = pqueue->min;
  //update pqueue data
  if(node->weight < curr->weight){
    pqueue->min = curr;
  }
  //find most common parent and attach
  //easiest case
  if(!curr->parent_->right_){
    curr->parent_->right_ = node;
    node->parent_ = curr->parent_;
  }
  //otherwise, find the next place you can put the new leaf
  else{ //just find a leaf - fix this later
    curr = head;
    while(curr->left_ || curr->right_){
      if(curr->left_){
        curr = curr->left_;
      }
      else if(curr->right_){
        curr = curr->right_;
      }
    }
  }
  struct k_list * lca = pqueue->head;
  while(true){
    if()
  }

	//swap until curr is less than its parent
	while(curr->weight > curr->parent_->weight){
		curr = heap_swap(curr->parent_, curr);
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
  struct priority_queue * pqueue = kmalloc(sizeof(struct priority_queue *). GFP_KERNEL);
	pqueue-> head = kmalloc(sizeof(struct k_list *),GFP_KERNEL);
  pqueue->min = head;
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

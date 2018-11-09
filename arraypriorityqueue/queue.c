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

static inline void
max_heapify(void){
	int i = 1;
	int l,r;
	int largest = 0;

	if (pqueue->size <= 1)
		return;

	while(true){
		/* @FIXME: this is going into an infinite loop */
		largest = i;
		l = 2*i;
		r = 2*i + 1;
		pr_info("%s: largest = %d, l = %d, r = %d, size = %d\n",
			__func__, largest, l, r, pqueue->size);
		if((l<=pqueue->size) && pqueue->requests[l].weight>pqueue->requests[r].weight){
			largest = l;
		}
		if((r<=pqueue->size)&&pqueue->requests[r].weight>pqueue->requests[l].weight){
			largest = r;
		}
		if(largest == i){
			pr_info("finished heapify, root: %s", pqueue->requests[1].data);
			return;
		}
		heap_swap(i,largest);
		i = largest + 1;
		pr_info("end of loop - largest: %d",largest);
	}
}

//dequeue
ssize_t dequeue(struct file *filp,char *buf,size_t count,loff_t *offp){
	int de=0;
	if (*offp > 0){
		return 0;
	}
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
	pr_info("Checking new node\n");
	pr_info("New node check passed!\n");
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
		new_node=1;//there are more nodes to dequeue
	}
	//re-heapify the queue
	pqueue->requests[1].data ="NULL";
	pqueue->requests[1].weight = -1;
	//first put the last one in the first one's spot
	heap_swap(1,pqueue->size);
	pqueue->size = pqueue->size-1;
	//call the heapify function
	max_heapify();
	//return count;
	*offp += count;
	return count;
}

//enqueue
ssize_t enqueue(struct file *filp,const char *buf,size_t count,loff_t *offp){
	if(pqueue->size == pqueue->max_size){//if it is full
		pr_info("Queue is full\n");
		return count;
	}
	msg=kmalloc(10*sizeof(char),GFP_KERNEL);
	temp=copy_from_user(msg,buf,count);
	msg[count] = '\0';
	/* @FIXME: where do you want to insert? */
	pqueue->requests[(pqueue->size)].data=msg;
	pqueue->requests[pqueue->size].weight = _weight;
	(pqueue->size)++;
	_weight ++;
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
	proc_create("queue",0666,NULL,&proc_fops);
}
//initialize the queue!
int queue_init (void) {
	unsigned q = 0;
	create_new_proc_entry();
	pqueue = kmalloc(sizeof(struct priority_queue),GFP_KERNEL);
	pqueue->size = 0;
	pqueue->max_size = 6;
	pqueue->requests = kcalloc(6, sizeof(struct k_list),GFP_KERNEL);
	//initialize all of the requests to have values so that max_heapify works
	q = 0;
	while(q<6){
		pqueue->requests[q].data = NULL;
		pqueue->requests[q].weight = -1;
		q++;
	}
	emp_len=strlen(empty);
	printk("init queue");
	return 0;
}

static inline void
cleanup_queue(void)
{
	int i;
	if (pqueue && pqueue->size > 0) {
		for (i = 0;i < pqueue->size; ++i) {
			if (pqueue->requests[i].data){
				kfree(pqueue->requests[i].data);
			}
		}
		kfree(pqueue->requests);
		kfree(pqueue);
	}
}

//delete everything
void queue_cleanup(void) {
	cleanup_queue();
	remove_proc_entry("queue",NULL);
	printk("cleanin' queue");
}

module_init(queue_init);
module_exit(queue_cleanup);

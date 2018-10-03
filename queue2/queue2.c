#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

//create structure for queue and data
struct my_struct{
    int n;
    struct list_head node;
}

SYSCALL_DEFINE0(list_test){
    
    int i;
    struct list_head *h;
    struct list_head *last;
    LIST_HEAD(head);

    printk("creating queue");
    
    //initial values when you only have a head
    head.prev = &head;
    head.next = &head;
    //add things to the queue

    for(i=0;i<3;i++){
        //add to the queue
        list_add_tail(&new_node->node,&head);    
    }

    list_for_each(h,&head){
        struct my_struct *element = list_entry(h, struct my_struct,node);
        printk("%d ", element->n);
    }

    //clean memory
    list_for_each(h,&head){
        struct my_struct *element = list_entry(h,struct my_struct, node);
        kfree(element);
    }

    return 0;
}

struct my_struct{
    int n;
    struct list_head node;
};

SYSCALL_DEFINE0(list_test){
    int i;
    struct list_head *h;
    struct list_head *last;
    LIST_HEAD(head);
 
    //Make a stack 
    printk("Making a stack\n");    
    for (i=0;i<3;i++){ //Let's only add 3 members struct my_struct *new_node = (struct my_struct *)kmalloc(sizeof(struct my_struct),GFP_KERNEL); new_node->n = (i+1);
        list_add(&new_node->node,&head);
        
    }
    //now read the memebers
    printk("Members: \n");    
    list_for_each(h,&head){
        struct my_struct *element = list_entry(h,struct my_struct,node);
        printk("%d ",element->n);
    }
    //clean up
    list_for_each(h,&head){
        struct my_struct *element = list_entry(h,struct my_struct,node);
        kfree(element);
    }
    
    //Now make a queue
    printk("Making a queue/method 1\n");
    head.prev=&head;
    head.next=&head;
    for (i=0;i<3;i++){ //Let's only add 3 members struct my_struct *new_node = (struct my_struct *)kmalloc(sizeof(struct my_struct),GFP_KERNEL); new_node->n = (i+1);
        list_add_tail(&new_node->node,&head);
        
    }
    //now read the memebers
    printk("Members: \n");    
    list_for_each(h,&head){
        struct my_struct *element = list_entry(h,struct my_struct,node);
        printk("%d ",element->n);
    }
    //clean up
    list_for_each(h,&head){
        struct my_struct *element = list_entry(h,struct my_struct,node);
        kfree(element);
    }
    
    
    
    printk("Making a queue/method 2\n");
    head.prev=&head;
    head.next=&head;
    last=&head;
    for (i=0;i<3;i++){ //Let's only add 3 members struct my_struct *new_node = (struct my_struct *)kmalloc(sizeof(struct my_struct),GFP_KERNEL); new_node->n = (i+1);
        list_add(&new_node->node,last);
        last=&new_node->node;
    }
    //now read the memebers
    printk("Members: \n");    
    list_for_each(h,&head){
        struct my_struct *element = list_entry(h,struct my_struct,node);
        printk("%d ",element->n);
    }
    //clean up
    list_for_each(h,&head){
        struct my_struct *element = list_entry(h,struct my_struct,node);
        kfree(element);
    }
    printk("returning");
    return 0;
}

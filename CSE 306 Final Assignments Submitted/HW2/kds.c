#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* KERN_INFO */
#include <linux/init.h> /* Init and exit macros */
#include <linux/string.h>
#include <linux/list.h>
#include<linux/slab.h>
#include<linux/hashtable.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <linux/blk-cgroup.h>
#include <trace/events/block.h>
#include <linux/percpu.h>
#include <linux/elevator.h>
#include <linux/rbtree.h>
#include <linux/radix-tree.h>
#include <linux/xarray.h>

/* ... */


/*
-------------------------------------------------------------------------------------------------------------
			Cmd line input strings 
-------------------------------------------------------------------------------------------------------------
*/

static char *int_string = "default value";
static char *linked_string, *bitmap_string, *hashmap_string, *rbtree_string, *radix_string, *xarray_string;

module_param(int_string, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(int_string, "Another parameter, a string");

module_param(linked_string, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(linked_string, "Another parameter, a string");

module_param(bitmap_string, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(bitmap_string, "Another parameter, a string");

module_param(hashmap_string, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(hashmap_string, "Another parameter, a string");

module_param(rbtree_string, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(rbtree_string, "Another parameter, a string");

module_param(radix_string, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(radix_string, "Another parameter, a string");

module_param(xarray_string, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(xarray_string, "Another parameter, a string");

char *token;
int val,err, val2;


/*
-------------------------------------------------------------------------------------------------------------
			Function Prototypes 
-------------------------------------------------------------------------------------------------------------
*/

int kds_linkedlist(char *linked_string);
int kds_bitmap(char *bitmap_string);
int kds_hashmap(char *hashmap_string);
int kds_rbtree(char *rbtree_string);
int kds_radixtree(char *radix_string);
int kds_xarray(char *xarray_string);

/*
-------------------------------------------------------------------------------------------------------------
			init function defined here 
-------------------------------------------------------------------------------------------------------------
*/

static int __init kds_init(void)
{
/* Function takes in a string of numbers separted by spaces and prints them one after the other after converting them to int*/ 
printk("Init entered \n"); 


	while ( (token = strsep(&int_string, " ")) ) 
	{
	    err = kstrtoint(token, 10, &val);
	    if(!err) 
	    {
	      	printk("Values from Init %d\n", val);
	    }
	}

kds_linkedlist(linked_string);
kds_bitmap(bitmap_string);
kds_hashmap(hashmap_string);
kds_rbtree(rbtree_string);
kds_radixtree(radix_string);
kds_xarray(xarray_string);

return 0;
}

/*
-------------------------------------------------------------------------------------------------------------
			linked list function defined here 
-------------------------------------------------------------------------------------------------------------
*/

int kds_linkedlist(char *linked_string)
{
 
/* Defining the struct for the Linked List */
struct my_ll_struct { int data ; struct list_head mylist ; } ;
/* Initialising the Head Node */
LIST_HEAD(Head_Node);

/*Creating Nodes as we get inputs */
struct my_ll_struct *tmp_node;

struct my_ll_struct *curr;
struct my_ll_struct *temp;


printk("Linked list function called \n");   
	while ( (token = strsep(&linked_string, " ")) ) 
	{   
	    err = kstrtoint(token, 10, &val);
	    if(!err) 
	    {
	      	tmp_node = kmalloc(sizeof(struct my_ll_struct), GFP_KERNEL);
		tmp_node->data = val;

		INIT_LIST_HEAD(&tmp_node->mylist);
		list_add(&tmp_node->mylist, &Head_Node);
	    }
	}
	
	list_for_each_entry(curr, &Head_Node, mylist) 
	{
	printk(KERN_INFO "Linked List Val: %d\n", curr->data);
	}
	
        list_for_each_entry_safe(curr, temp, &Head_Node, mylist) {
            list_del(&curr->mylist);
            kfree(curr);
        }
       
        return 0;
}

/*
-------------------------------------------------------------------------------------------------------------
			xarray function defined here 
-------------------------------------------------------------------------------------------------------------
*/

int kds_xarray(char *xarray_string)
{
void *entry;



int buff[10];
int i = 0;
int count = 0;
unsigned long a;
DEFINE_XARRAY(my_xarray);
for (i=0;i<10;i++)
{
buff[i] = 0;
}
printk(KERN_INFO "Xarray function called\n");


	while ( (token = strsep(&xarray_string, " ")) ) 
	{
	    err = kstrtoint(token, 10, &val);
	    if(!err) 
	    {	
		entry = &val;
		xa_store(&my_xarray, val, entry,GFP_KERNEL);
		buff[count] = val;
		count++;
		a = buff[count];
		xa_for_each(&my_xarray,a , entry) {
		//printk(KERN_INFO "Xarray val %d\n", *(int *) entry)  ;
	    }
	}		
}
return 0;
}

/*
-------------------------------------------------------------------------------------------------------------
			radix tree function defined here 
-------------------------------------------------------------------------------------------------------------
*/

int kds_radixtree(char *radix_string)
{
void *item;


void *tmp;
int buff[10];
int i = 0;
int count = 0;
int a;
RADIX_TREE(radixtree, GFP_KERNEL);
//INIT_RADIX_TREE(&radixtree, GFP_KERNEL);

printk(KERN_INFO "Radix tree function called\n");

for (i=0;i<10;i++)
{
buff[i] = 0;
}
	while ( (token = strsep(&radix_string, " ")) ) 
	{
	    err = kstrtoint(token, 10, &val);
	    if(!err) 
	    {	
		item = &val;
		radix_tree_preload(GFP_KERNEL);
		a = radix_tree_insert(&radixtree, val, item);
		if (a) {return -ENOMEM;}
		radix_tree_preload_end();
		buff[count] = val;
		count++;
		tmp = radix_tree_lookup(&radixtree, val);
		printk(KERN_INFO "Radix tree val %d\n", *(int *) tmp)  ;
	    }
	}
	
	for (i=0;i<count;i++)
	{
		tmp = radix_tree_lookup(&radixtree, buff[i]);
		//printk(KERN_INFO "Buff val %d\n",buff[i])  ;
		printk(KERN_INFO "Radix tree val %d\n", *(int *) tmp)  ;
	}


return 0;
}

/*
-------------------------------------------------------------------------------------------------------------
			RB Tree function defined here 
-------------------------------------------------------------------------------------------------------------
*/

int kds_rbtree(char *rbtree_string)
{

//sched_entity
struct my_rb_struct 
{
	int data;
	struct rb_node run_node;
};

// cfs_rq
struct my_rb_root_struct {
	int data;
	struct rb_root run_node;
};

struct my_rb_struct *rb_tree; //sched_entity
struct my_rb_root_struct *rb_tree_root; //cfs_rq
//struct my_rb_struct *tmp_node;
//struct my_rb_root_struct *rb;
struct rb_node *node;
struct my_rb_struct *data; 

int buff[10];
int i = 0;
int count = 0;

void enqueue_entity(struct my_rb_root_struct *rb_tree_root, struct my_rb_struct *rb_tree)
{
	struct rb_node **link = &rb_tree_root->run_node.rb_node; /* root node */
	struct rb_node *parent = NULL;
	struct my_rb_struct *entry;
	/* Traverse the rbtree to find the right place to insert */
	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct my_rb_struct, run_node);
		if (rb_tree->data < entry->data) {
			link = &parent->rb_left;
		} else {
		link = &parent->rb_right;
		}
	}
	/* Insert a new node */
	rb_link_node(&rb_tree->run_node, parent, link);
	/* Re-balance the rbtree if necessary */
	rb_insert_color(&rb_tree->run_node, &rb_tree_root->run_node);
}

	struct my_rb_struct *my_search(struct rb_root *root, int val)
	{
  		struct rb_node *node = root->rb_node;

  		while (node) 
  		{
  			struct my_rb_struct *data = container_of(node, struct my_rb_struct, run_node);

			if ( val < data->data)
  				node = node->rb_left;
			else if (val > data->data)
  				node = node->rb_right;
			else
  				return data;
		}
		return NULL;
  	}

rb_tree_root = kmalloc(sizeof(struct my_rb_root_struct), GFP_KERNEL);
rb_tree_root->run_node = RB_ROOT;

printk(KERN_INFO "Rbtree function called\n");

for (i=0;i<10;i++)
{
buff[i] = 0;
}



	while ( (token = strsep(&rbtree_string, " ")) ) 
		{   
		    err = kstrtoint(token, 10, &val);
		    if(!err) 
		    {
		      	rb_tree = kmalloc(sizeof(struct my_rb_struct), GFP_KERNEL);
			rb_tree->data = val;
			buff[count] = val; count++;
			enqueue_entity(rb_tree_root, rb_tree);
		    }
		}
		
  		for (node = rb_first(&rb_tree_root->run_node); node; node = rb_next(node))
		{
			printk("Rb Tree Value : %d\n", rb_entry(node, struct my_rb_struct, run_node)->data);
			//buff[i] = rb_entry(node, struct my_rb_struct, run_node)->data;
			//i= i+1;
		}
		
		while(count>=0)
		{
			data = my_search(&rb_tree_root->run_node, buff[count]);
		  	if (data) {
		  		rb_erase(&data->run_node, &rb_tree_root->run_node);
		 	 	kfree(data);
		  	}
		  	count--;
		}
return 0;
}

/*
-------------------------------------------------------------------------------------------------------------
			hashmap function defined here
-------------------------------------------------------------------------------------------------------------
*/
int kds_hashmap(char *hashmap_string)
{

struct my_hashmap_struct {
     int data ;
     struct hlist_node my_hash_list ;
} ;
struct my_hashmap_struct *obj;
struct my_hashmap_struct *node;
/*struct elevator_queue *eq;
struct elevator_type *e;
struct request_queue *q;
static struct kobj_type elv_ktype; */

DEFINE_HASHTABLE(hash, 10);
int bucket;
int i;
hash_init(hash);
printk(KERN_INFO "Hashmap function called\n");
bucket = 0;
	while ( (token = strsep(&hashmap_string, " ")) ) 
	{   
	    err = kstrtoint(token, 10, &val);
	    if(!err) 
	    {
	      	
/*		eq = kzalloc_node(sizeof(*eq), GFP_KERNEL, 0);
		if (unlikely(!eq)) {return -1;}
		eq->type = e;
		kobject_init(&eq->kobj, &elv_ktype);
		mutex_init(&eq->sysfs_lock);
		
*/		

//		hash_init(eq->hash);

		node = kmalloc(sizeof(struct my_hashmap_struct), GFP_KERNEL);
		node->data = val;

		INIT_HLIST_NODE(&node->my_hash_list);
		hash_add(hash, &node->my_hash_list, val);
	    }
	}
	

		hash_for_each(hash, bucket, obj, my_hash_list)
		{
	        printk(KERN_INFO "Hashmap data : %d \n", obj->data);
		}
		for( i = 0; i < 21; i = i + 1 )
		{
      			hash_for_each_possible(hash, obj, my_hash_list, i)
      			{
			printk(KERN_INFO "Hashmap data within bucket %d: %d", i, obj->data);	
			}
		}
		
		hash_for_each(hash, bucket, obj, my_hash_list)
		{
	        	hash_del(&obj->my_hash_list);
	        	kfree(obj);
		}
		
return 0;

}

/*
-------------------------------------------------------------------------------------------------------------
			bitmap function defined here
-------------------------------------------------------------------------------------------------------------
*/
int kds_bitmap(char *bitmap_string)
{
int data;
DECLARE_BITMAP(my_bitmap,1001);
bitmap_zero(my_bitmap, 1001);

printk(KERN_INFO "Bitmap function called\n");

while ( (token = strsep(&bitmap_string, " ")) ) 
	{   
	    err = kstrtoint(token, 10, &val);
	    if(!err) 
	    {
	      	set_bit(val, my_bitmap);
	    }
	}

	for_each_set_bit(data,my_bitmap,1001)
	{
		printk(KERN_INFO "Bitmap Data: %d\n", data);
	}	

bitmap_zero(my_bitmap, 1001);

return 0;
}

/*
-------------------------------------------------------------------------------------------------------------
			exit function defined here
-------------------------------------------------------------------------------------------------------------
*/

static void __exit kds_exit(void)
{
printk(KERN_INFO "Module exiting ...\n");
}

/*
-------------------------------------------------------------------------------------------------------------
			Calling the Init and exit functions
-------------------------------------------------------------------------------------------------------------
*/

module_init(kds_init); /* lkp_init() will be called at loading the module */
module_exit(kds_exit); /*lkp_exit() will be called at unloading the module */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Deven Diwakar <ddiwakar@cs.stonybrook.edu");
MODULE_DESCRIPTION("Kernel module takes in a string of numbers separted by spaces and prints them one after the other after converting them to int");

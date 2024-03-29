#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>

struct mydata {
  struct task_struct * prev;
  unsigned long long prev_rdtsc;
};

struct myhashnode {
    struct hlist_node hash;
    int value;
    unsigned long long tsc;
};

struct myrbtree {
    struct rb_node node;
    int value;
    unsigned long long tsc;
};

struct rb_root myroot = RB_ROOT;

static char func_name[NAME_MAX] = "pick_next_task_fair";
module_param_string(func, func_name, NAME_MAX, S_IRUGO);
MODULE_PARM_DESC(func, "Function to kretprobe; this module will report the context switches ");

atomic_t pre_count = ATOMIC_INIT(0);
atomic_t post_count = ATOMIC_INIT(0);
atomic_t context_switch_count = ATOMIC_INIT(0);

DEFINE_SPINLOCK(my_lock);
DEFINE_HASHTABLE(myhashtable, 16);

static unsigned long long search_hash_node(int number) {
    struct myhashnode * curr;
    int key = number;
    hash_for_each_possible(myhashtable, curr, hash, key)
    {
        if(curr->value == number)
            return curr->tsc;
    }
    return 0;
}

static void remove_hash_node(int number) {
    struct myhashnode * curr;
    int key = number;
    hash_for_each_possible(myhashtable, curr, hash, key)
    {
        if(curr->value == number) {
            printk(KERN_INFO "Deleting : %d \n", curr->value);
            hash_del(&curr->hash);
            kfree(curr);
        }
    }
}

static void insert_hash_node(int pid, unsigned long long input_tsc) {
    struct myhashnode * mynode;

    remove_hash_node(pid);
    
    mynode = (struct myhashnode *)kmalloc(sizeof(struct myhashnode), GFP_ATOMIC);
    mynode->value = pid;
    mynode->tsc = input_tsc;
    hash_add(myhashtable, &mynode->hash, pid);
}

int insert_rb_tree_node(struct rb_root *root, struct myrbtree *new_node) {
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    
    while (*new) 
    {
        struct myrbtree *this = container_of(*new, struct myrbtree, node);
        parent = *new;
        if (new_node->tsc < this->tsc)
            new = &((*new)->rb_left);
        else if (new_node->tsc > this->tsc)
            new = &((*new)->rb_right);
        else
            return 0;
    }
    
    rb_link_node(&new_node->node, parent, new);
    rb_insert_color(&new_node->node, root);
    return 1;
}

void remove_rb_tree_node(struct rb_root * mytree, int pid) {
    struct rb_node *node;
    for (node = rb_first(mytree); node; node = rb_next(node))
    {
      struct myrbtree *temp = rb_entry(node, struct myrbtree, node); 
      if (temp->value == pid) 
      {
        rb_erase(&(temp->node), mytree); kfree(temp);  
      }
    }
}

void print_rb_tree(struct rb_root * mytree, struct seq_file *m) {
    struct rb_node *node;
    int count = 0;

    if(!rb_first(mytree)) 
    { printk(KERN_INFO "Root Node is null"); }
    
    for (node = rb_last(mytree); node; node = rb_prev(node)) 
    {
        struct myrbtree *temp = rb_entry(node, struct myrbtree, node);   
        seq_printf(m, "PID = %d , Total Tsc =  %llu \n", temp->value, temp->tsc);
        if (count++ > 8) { break; }  /*  count will be incremented to 9 and that constitutes 10 keys */
    }

}

void insert_redblack_trees(int pid, unsigned long long tsc) {
    struct myrbtree * mynode;
    mynode = (struct myrbtree *)kmalloc(sizeof(struct myrbtree), GFP_ATOMIC);
    mynode->value = pid;
    mynode->tsc = tsc;
    insert_rb_tree_node(&myroot, mynode);
}

unsigned long long search_rbtree(struct rb_root *mytree, int pid) {
    struct rb_node *node;
    for (node = rb_first(mytree); node; node = rb_next(node)) 
    {
      struct myrbtree *temp = rb_entry(node, struct myrbtree, node); 
      if (temp->value == pid) { return temp->tsc; }
    }
    return 0;
}

static int perftop_show(struct seq_file *m, void *v) 
{
	seq_printf(m, "pre_count is %d , post_count is %d, context_switch_count is %d\n" , atomic_read(&pre_count), atomic_read(&post_count), atomic_read(&context_switch_count) );
	spin_lock(&my_lock);
  	print_rb_tree(&myroot, m);
  	spin_unlock(&my_lock);
	return 0;
}

static int perftop_open(struct inode *inode, struct  file *file) 
{
	return single_open(file, perftop_show, NULL);
}

static int entry_pick_next_fair(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct mydata *data;
	struct task_struct * prev_task = (struct task_struct*)(regs->si);

	if (!current->mm) {return 1;}

  	data = (struct mydata *)ri->data;
	if (data != NULL)
	{
		data->prev = prev_task;
	}

	atomic_inc(&pre_count);

  return 0;

}
NOKPROBE_SYMBOL(entry_pick_next_fair);

static int ret_pick_next_fair(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct mydata *data = (struct mydata *)ri->data;
	struct task_struct * next = (struct task_struct*)(regs->ax);
	unsigned long long sum = 0;
	atomic_inc(&post_count);
	  
	if (data != NULL && data->prev != NULL && next != NULL && data->prev!= next) 
	{
	      unsigned long long curr =  rdtsc();
	      unsigned long long old = search_hash_node(data->prev->pid);
	      
	      if (old != 0) 
	      {
		remove_hash_node(data->prev->pid);
		spin_lock(&my_lock);
		sum = search_rbtree(&myroot, data->prev->pid);
		sum = sum + curr - old;
		pr_info("Inserting PID : %d TSC : %llu \n", data->prev->pid, sum);
		remove_rb_tree_node(&myroot, data->prev->pid);
		insert_redblack_trees(data->prev->pid, sum);
		spin_unlock(&my_lock);
	      }
	      insert_hash_node(next->pid, curr);
	      atomic_inc(&context_switch_count);
	}
	return 0;
	  
}
NOKPROBE_SYMBOL(ret_pick_next_fair);

static struct kretprobe my_kretprobe = 
{
  .handler    = ret_pick_next_fair,
  .entry_handler    = entry_pick_next_fair,
  /* Probe up to 20 instances concurrently. */
  .maxactive    = 20,
};

static const struct proc_ops perftop_fops = 
{
  .proc_open = perftop_open,
  .proc_read = seq_read,
  .proc_lseek = seq_lseek,
  .proc_release = single_release,
};

static int __init perftop_init(void) 
{
  int ret;
  proc_create("perftop", 0, NULL, &perftop_fops);
  hash_init(myhashtable);
  my_kretprobe.kp.symbol_name = func_name;
  ret = register_kretprobe(&my_kretprobe);
  if (ret < 0) 
  {
    pr_err("register_kretprobe failed, returned %d\n", ret);
    return -1;
  }
  pr_info("Planted return probe at %s: %p\n", my_kretprobe.kp.symbol_name, my_kretprobe.kp.addr);
  
  return 0;
}

static void __exit perftop_exit(void) 
{
  struct myhashnode * curr;
  struct rb_node *node;
  int key = 0;
  remove_proc_entry("perftop", NULL);
  unregister_kretprobe(&my_kretprobe);
  pr_info("kretprobe at %p unregistered\n", my_kretprobe.kp.addr);
   	    
	    hash_for_each_possible(myhashtable, curr, hash, key)
	    {
	      hash_del(&curr->hash); kfree(curr);
	    }

	    for (node = rb_first(&myroot); node; node = rb_next(node))
	    {
	      struct myrbtree *temp = rb_entry(node, struct myrbtree, node); 
	      rb_erase(&(temp->node), &myroot);
	      kfree(temp);    
	    }
   
  /* nmissed > 0 suggests that maxactive was set too low. */
  pr_info("Missed probing %d instances of %s\n", my_kretprobe.nmissed, my_kretprobe.kp.symbol_name);
}

MODULE_LICENSE("GPL");
module_init(perftop_init);
module_exit(perftop_exit);

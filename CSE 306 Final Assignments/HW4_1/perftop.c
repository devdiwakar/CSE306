#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/sched.h>

struct mydata 
{
  struct task_struct * prev;
};

static char func_name[NAME_MAX] = "pick_next_task_fair";
module_param_string(func, func_name, NAME_MAX, S_IRUGO);
MODULE_PARM_DESC(func, "Function to kretprobe; this module will report the context switches ");

atomic_t pre_count = ATOMIC_INIT(0);
atomic_t post_count = ATOMIC_INIT(0);
atomic_t context_switch_count = ATOMIC_INIT(0);

static int perftop_show(struct seq_file *m, void *v) 
{
	seq_printf(m, "pre_count is %d , post_count is %d, context_switch_count is %d\n" , atomic_read(&pre_count), atomic_read(&post_count), atomic_read(&context_switch_count) );
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
	struct task_struct * next_task = (struct task_struct*)(regs->ax);
  
	atomic_inc(&post_count);
	  
	if (data != NULL && data->prev != NULL && next_task != NULL && data->prev!= next_task) 
	{
		pr_info("Context switch detected; incrementing context_switch_count \n");
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
  remove_proc_entry("perftop", NULL);
  unregister_kretprobe(&my_kretprobe);
  pr_info("kretprobe at %p unregistered\n", my_kretprobe.kp.addr);
  /* nmissed > 0 suggests that maxactive was set too low. */
  pr_info("Missed probing %d instances of %s\n", my_kretprobe.nmissed, my_kretprobe.kp.symbol_name);
}

MODULE_LICENSE("GPL");
module_init(perftop_init);
module_exit(perftop_exit);

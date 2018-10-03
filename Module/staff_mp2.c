#define pr_fmt(fmt) "cs423_mp2: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>

#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("CS423Staff");
MODULE_DESCRIPTION("mp2 sample solution");

/* type definitions */
struct mp2_task_struct {
	struct task_struct *linux_task;

	pid_t pid;
	struct timer_list wakeup_timer;
	unsigned long period_ms;
	unsigned long comptime_ms;
	unsigned long deadline_jiff;
	int state;

	struct list_head llist;
};

enum mp2_task_state {
	MP2_READY,
	MP2_SLEEPING,
	MP2_RUNNING,

	MP2_LASTONE, /* this should always be the last one */
};

#define UTIL_RATIO 6930
#define MULTIPLIER 10000

/* global variables */

// initialize the head of the list
static LIST_HEAD(head);

// this is for the proc entries that we want
static struct proc_dir_entry *mp2_proc_parent, *mp2_proc_entry;

static struct kmem_cache *mp2_cache;
static DEFINE_SPINLOCK(mp2_lock); /* spinlock for the linked list */
static DEFINE_MUTEX(mp2_mutex); /* mutex for the current task */
static struct task_struct *mp2_dispatcher_th;
static struct mp2_task_struct *mp2_current_task;

static const char *mp2_states[] = {"READY", /* MP2_READY */
	"SLEEPING", /* MP2_SLEEPING */
	"RUNNING"   /* MP2_RUNNING */};

#define log_state_change(pid, s, ns) \
	pr_info("State of application %i changed from %s to %s...", \
		pid, mp2_states[s], mp2_states[ns]);

#define MP2_SCHED_TASK(mp2_task, priority, sched_type, sparam) \
	sparam.sched_priority = priority;                      \
	sched_setscheduler(mp2_task->linux_task, sched_type, &sparam);

static inline void mp2_update_task_state(struct mp2_task_struct *mp2_task,
					 int new_state)
{
	log_state_change(mp2_task->pid, mp2_task->state, new_state);

	spin_lock_bh(&mp2_lock);
	mp2_task->state = new_state;
	spin_unlock_bh(&mp2_lock);
}

/**
 * __lookup_task - lookup a task within our list
 * This assumes that it holds the lock on the list
 *
 * @pid: the pid of the struct we're looking for
 */
static struct mp2_task_struct *__lookup_task(pid_t pid)
{
	struct mp2_task_struct *next;

	list_for_each_entry(next, &head, llist) {
		if (next->pid == pid)
			return next;
	}

	return NULL;
}


/**
 * mp2_timer_callback - the handler for the timer for our processes
 * @data: the pid of the process that expired
 *
 * Description: the handler should change the state of the task to READY
 * and should wake up the dispatching thread.
 */
static void mp2_timer_callback(unsigned long data)
{
	struct mp2_task_struct *mp2_task;

	spin_lock_bh(&mp2_lock);
	mp2_task = __lookup_task((pid_t)data);
	spin_unlock_bh(&mp2_lock);

	/* this should never happen, means something went really wrong */
	BUG_ON(mp2_task == 0);

	mp2_update_task_state(mp2_task, MP2_READY);

	wake_up_process(mp2_dispatcher_th);
}

/**
 * lookup_high_prior_task - lookup the task with the highest priority
 *
 * Description: Look for the READY task that has the highest priority
 * among the registered tasks.
 */
static struct mp2_task_struct *lookup_high_prior_task(void)
{
	struct mp2_task_struct *next=0, *candidate=0;
	unsigned long high_priority = ULONG_MAX;

	spin_lock_bh(&mp2_lock);
	list_for_each_entry(next, &head, llist) {
		if (next->state == MP2_READY &&
		    next->period_ms < high_priority) {
			candidate = next;
			high_priority = candidate->period_ms;
		}
	}
	spin_unlock_bh(&mp2_lock);

	return candidate;
}


/**
 * mp2_dispactcher_fn - Our scheduler's dispatcher thread handler
 *
 * Description: As soon as the context switch wakes up, you will need
 * to find in the list, the task with READY state that has the highest
 * priority. Then you need to preempt the currently running task. If
 * there are no tasks in READY state, keep the current one running, if
 * any. The task state of the old task must be set to READY only if the
 * state is RUNNING. Also you must set the state of the new running task
 * to RUNNING.
 */
static int mp2_dispatcher_fn(void *data)
{
	struct mp2_task_struct *next_candidate = 0;
	struct sched_param curr_sparam, next_sparam;

	set_current_state(TASK_INTERRUPTIBLE);
	while (!kthread_should_stop()) {
		/* go to sleep and trigger context switch */
		schedule();

		/* to avoid missed wakeup call */
		set_current_state(TASK_INTERRUPTIBLE);

		pr_debug("Dispatcher thread activated");
		next_candidate = lookup_high_prior_task();
		if (next_candidate) {
			/* found next candidate with highest priority */
			mutex_lock_interruptible(&mp2_mutex);
			if ((!mp2_current_task) ||
			    (mp2_current_task &&
			     (next_candidate->period_ms <
				     mp2_current_task->period_ms)))
			{
				/* demote the current task if any */
				if (mp2_current_task) {
					MP2_SCHED_TASK(mp2_current_task, 0, SCHED_NORMAL,
						       curr_sparam);
					pr_debug("preempting task with pid=%i\n",
						 mp2_current_task->pid);

					mp2_update_task_state(mp2_current_task,
							      MP2_READY);
					set_task_state(mp2_current_task->linux_task,
						 TASK_UNINTERRUPTIBLE);
				}

				/* wake up the sleeping process */
				pr_debug("waking up process %i",
					 next_candidate->pid);
				MP2_SCHED_TASK(next_candidate, 98, SCHED_FIFO,
					       next_sparam);
				mp2_update_task_state(next_candidate,
						      MP2_RUNNING);
				wake_up_process(next_candidate->linux_task);

				mp2_current_task = next_candidate;
			}
			mutex_unlock(&mp2_mutex);
		}
	}

	__set_current_state(TASK_RUNNING);
	pr_debug("Goodbye from dispatcher thread!");
	return 0;
}

/**
 * Sequential file functions needed to read from the module's proc entry
 */
static void *mp2_start(struct seq_file *s, loff_t *pos)
{
	/* should acquire locks here for the whole reading operation */
	spin_lock_bh(&mp2_lock);
	return seq_list_start(&head, *pos);
}

static void *mp2_next(struct seq_file *s, void *v, loff_t *pos)
{
	return seq_list_next(v, &head, pos);
}

static void mp2_stop(struct seq_file *s, void *v)
{
	/* release lock here */
	spin_unlock_bh(&mp2_lock);
}

static int mp2_show(struct seq_file *s, void *v)
{
	struct mp2_task_struct *entry = list_entry((struct list_head *)v,
						   struct mp2_task_struct,
						   llist);

	seq_printf(s, "%d;%lu;%lu;%s\n",
		  entry->pid,
		  entry->period_ms,
		  entry->comptime_ms,
		  mp2_states[entry->state]);

	return 0;
}

/* sequential file operations */
static const struct seq_operations mp2_seq_ops = {
	.start = mp2_start,
	.next  = mp2_next,
	.stop  = mp2_stop,
	.show  = mp2_show,
};

static int mp2_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &mp2_seq_ops);
}

/**
 * mp2_create_task - creates a custom task struct for our scheduler
 * @linux_task: the task_struct corresponding to the registering process
 * @pid: the process's pid
 * @period: the process's period requirement
 * @computation: the process's computation time requirement
 */
static inline struct mp2_task_struct
*_mp2_create_task(struct task_struct *linux_task, pid_t pid,
		 unsigned long period, unsigned long computation)
{
	struct mp2_task_struct *mp2_task;

	/* create a new custom task and initialize it */
	mp2_task = (struct mp2_task_struct *)
		kmem_cache_alloc(mp2_cache, GFP_KERNEL);
	if (IS_ERR(mp2_task)) {
		pr_err("failed to allocate new task struct");
		return mp2_task;
	}

	/* fill in the needed stuff and set up the timer */
	mp2_task->pid = pid;
	mp2_task->linux_task = linux_task;
	mp2_task->period_ms = period;
	mp2_task->comptime_ms = computation;
	mp2_task->deadline_jiff = 0;
	mp2_task->state = MP2_SLEEPING;

	setup_timer(&mp2_task->wakeup_timer, mp2_timer_callback, mp2_task->pid);

	/* add to the list of processes */
	spin_lock_bh(&mp2_lock);
	list_add_tail(&(mp2_task->llist), &head);
	spin_unlock_bh(&mp2_lock);

	return mp2_task;
}

/**
 * check_admission_control - Checks whether we should accept a new process
 * @period: the period of the new task to register
 * @computation: the computation time for the new task
 *
 * Description: Performs the admission control check according to the condition
 * presented in the original RMS paper.
 */
static int check_admission_control(unsigned long period,
				   unsigned long computation)
{
	unsigned long sum = 0;
	struct mp2_task_struct *next;

	spin_lock_bh(&mp2_lock);
	list_for_each_entry(next, &head, llist) {
		sum += (next->comptime_ms * MULTIPLIER) / next->period_ms;
	}
	spin_unlock_bh(&mp2_lock);

	sum += (computation * MULTIPLIER) / period;

	if (sum <= UTIL_RATIO) {
		pr_debug("Admission control check passed");
		return 1;
	}

	pr_err("Admission control check failed");
	return 0;
}

/**
 * process_register - handler for the registration
 * @pid: the pid of the task to register
 * @period: the required period for the process
 * @computation: the required computation time for the process
 *
 * Description: Implement registration for new processes into the
 * system, but only after they pass admission control
 */
static int process_register(pid_t pid,
			    unsigned long period, unsigned long computation)
{
	struct mp2_task_struct *mp2_task;
	struct task_struct *linux_task;

	/* admission control should happen at this step */
	if (!check_admission_control(period, computation))
		return -EINVAL;

	linux_task = find_task_by_pid(pid);
	if (!linux_task) {
		pr_err("invalid process id given");
		return -EINVAL;
	}

	mp2_task = _mp2_create_task(linux_task, pid, period, computation);
	if (IS_ERR(mp2_task))
		return PTR_ERR(mp2_task);

	return 0;
}

/**
 * __lookup_and_delete_task - lookup and remove a task from the list
 *
 * Description: A slight modification to _lookup_task to improve
 * on the use of locks
 */
static struct mp2_task_struct *__lookup_and_delete_task(pid_t pid)
{
	struct mp2_task_struct *next, *temp;

	list_for_each_entry_safe(next, temp, &head, llist) {
		if (next->pid == pid) {
			list_del(&(next->llist));
			return next;
		}
	}

	return NULL;
}

/**
 * process_deregister - handler for deregistration
 * @pid: the pid of the task to remove
 *
 * Description: The de-registration requires you to remove the task
 * from the list and free all data structures allocated during registration.
 */
static int process_deregister(pid_t pid)
{
	struct mp2_task_struct *task;

	spin_lock_bh(&mp2_lock);
	task = __lookup_and_delete_task(pid);
	spin_unlock_bh(&mp2_lock);
	if (!task) {
		pr_err("unrecognized task %d\n", pid);
		return -EINVAL;
	}

	mutex_lock_interruptible(&mp2_mutex);
	if (mp2_current_task == task) {
		mp2_current_task = NULL;
		wake_up_process(mp2_dispatcher_th);
	}
	mutex_unlock(&mp2_mutex);


	del_timer(&(task->wakeup_timer));
	kmem_cache_free(mp2_cache, task);

	pr_debug("Removed application with pid %d from the list!", pid);

	return 0;
}

/**
 * process_yield - handler for the yield call
 * @pid: the pid of the task issuing the yield request
 *
 * Description: In this function, we need to change the
 * state of the calling task to SLEEPING. We need to calculate
 * the next release time (that is the beginning of the next period),
 * we must set the timer and we must put the task to sleep as
 * TASK_UNINTERRUPTIBLE.
 */
static int process_yield(pid_t pid)
{
	struct mp2_task_struct *mp2_task;

	spin_lock_bh(&mp2_lock);
	mp2_task = __lookup_task(pid);
	spin_unlock_bh(&mp2_lock);
	if (!mp2_task) {
		pr_err("unrecognized task %d\n", pid);
		return -EINVAL;
	}

	pr_debug("Process %i yeilding\n", pid);
	if (mp2_task->state == MP2_SLEEPING) {
		/* first time yield call */
		mp2_task->deadline_jiff = jiffies
			+ msecs_to_jiffies(mp2_task->period_ms);
		mod_timer(&(mp2_task->wakeup_timer), mp2_task->deadline_jiff);
	} else {
		/* calculate the next timer wakeup and determine
		 * wether we should change the state of the process
		 */
		mp2_task->deadline_jiff += msecs_to_jiffies(mp2_task->period_ms);
		if (mp2_task->deadline_jiff > jiffies)
			mod_timer(&(mp2_task->wakeup_timer),
				  mp2_task->deadline_jiff);
		else {
			/* handle case where next period has already started
			 * if the task has not already been preempted, this
			 * means that is has the highest priority so it should
			 * keep running, so don't change its state and leave it
			 * running until it finishes or someone else preempts
			 * it.
			 */
			mp2_update_task_state(mp2_task, MP2_READY);
			goto sleep_mode;
		}
	}

	/* update the state of the task */
	mp2_update_task_state(mp2_task, MP2_SLEEPING);

	/* update current running task to NULL if calling task was the last one
	 * running
	 */
sleep_mode:
	mutex_lock_interruptible(&mp2_mutex);
	if (mp2_current_task == mp2_task)
		mp2_current_task = NULL;
	mutex_unlock(&mp2_mutex);

	/* wake up dispatcher */
	pr_debug("waking up the dispatcher thread after a yield call!");
	wake_up_process(mp2_dispatcher_th);

	/* put calling process to sleep */
	set_task_state(mp2_task->linux_task, TASK_UNINTERRUPTIBLE);
	schedule();

	return 0;
}

/**
 * mp2_write - the write handler for the procfs interface
 * @buffer: where the data comes from
 */
static ssize_t mp2_write(struct file *s, const char __user *buffer,
			 unsigned long count, loff_t *data)
{
	char *buff;
	int ret;
	char m;
	int pid;
	unsigned long period=0, computation=0;

	/* copy data from user land and null terminate it */
	buff = (char *)kmalloc(count+1, GFP_KERNEL);
	if (IS_ERR(buff)) {
		pr_err("out of memory, cannot allocate buffer...");
		return PTR_ERR(buff);
	}

    // copy content of buffer into the kernel
	if (copy_from_user(buff, buffer, count)) {
		pr_err("error copying message from user...");
		ret = -EFAULT;
		goto exit;
	}

	if (buff[count-1] == '\n') { /* remove trailing endline from echo */
		buff[count-1] = '\0';
	}
	buff[count] = '\0';


    // basically this is where pushing onto the queue shoudl be happening

    // don't worry about what's happening here
	ret = sscanf(buff, "%c%*[,] %d%*[,] %lu%*[,] %lu",
		     &m, &pid, &period, &computation);
	if (ret < 2) { /* need at least two of those arguments */
		pr_err("malformed command received...");
		ret = -EINVAL;
		goto exit;
	}

	switch(m) {
	case 'R':
		ret = process_register(pid, period, computation);
		break;
	case 'Y':
		ret = process_yield(pid);
		break;
	case 'D':
		ret = process_deregister(pid);
		break;
	default:
		pr_err("unrecognized command %s", buff);
		ret = -EINVAL;
		break;
	}

exit:
	kfree(buff);

	return (ret == 0)? count : ret;
}

/* file operations for interfacing with procfs */
static const struct file_operations mp2_file_ops = {
	.owner   = THIS_MODULE,
	.open    = mp2_open,
	.read    = seq_read,
	.write   = mp2_write,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static int __init mp2_init(void)
{
	int ret=0;
	struct sched_param sparam;

	pr_info("module loading...");

    // code we care about
	mp2_proc_parent = proc_mkdir("mp2", NULL);
	if (!mp2_proc_parent) {
		pr_err("failed to create /proc/mp2");
		ret = -ENOMEM;
		goto exit;
	}

	mp2_proc_entry = proc_create("status", 0666,
				     mp2_proc_parent, &mp2_file_ops);
	if (!mp2_proc_entry) {
		pr_err("failed to create procfs /proc/mp2/status entry...");
		ret = -ENOMEM;
		goto exit_on_procdir;
	}

    // end of code we care about

	mp2_dispatcher_th = kthread_create(&mp2_dispatcher_fn,
					   NULL, "mp2_dispatcher");
	if (IS_ERR(mp2_dispatcher_th)) {
		pr_err("failed to create mp2 kthread...");
		ret = PTR_ERR(mp2_dispatcher_th);
		goto exit_on_procdir;
	}
	sparam.sched_priority = 99;
	sched_setscheduler(mp2_dispatcher_th, SCHED_FIFO, &sparam);

	mp2_cache = kmem_cache_create("mp2_cache",
				      sizeof(struct mp2_task_struct),
				      0, 0, NULL);
	if (IS_ERR(mp2_cache)) {
		pr_err("failed to create cache for mp2");
		ret = PTR_ERR(mp2_cache);
		goto exit_on_thread;
	}

	mp2_current_task = 0;
	mutex_init(&mp2_mutex);
exit:
	return ret;

exit_on_thread:
	kthread_stop(mp2_dispatcher_th);
exit_on_procdir:
	proc_remove(mp2_proc_parent);

	return ret;
}

static void __exit mp2_cleanup(void)
{
	struct mp2_task_struct *next, *temp;

	/* will acquire mutex in here to prevent the case where
	 * the module is remove while someone is registering or doing
	 * something else to the list
	 */
	spin_lock_bh(&mp2_lock);
    // example of freeing up memory of a linked list
	list_for_each_entry_safe(next, temp, &head, llist) {
        // llist of of type list_head which is what list_del 
        // accepts
        // signature list_del(struct list_head *)
		list_del(&(next->llist));

        // don't worry about this
		del_timer(&(next->wakeup_timer));

        // this is where freeing happens
        // this is equivalent to kfree
		kmem_cache_free(mp2_cache, next);
	}
	spin_unlock_bh(&mp2_lock);

	kmem_cache_destroy(mp2_cache);

	/* signal the kthread to stop */
	kthread_stop(mp2_dispatcher_th);

	mutex_destroy(&mp2_mutex);

    // cleanup all the proc entries we added in the init function
	proc_remove(mp2_proc_entry);
	proc_remove(mp2_proc_parent);
}

module_init(mp2_init);
module_exit(mp2_cleanup);

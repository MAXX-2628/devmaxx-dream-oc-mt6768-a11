#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/fs.h>

#define MAX_BLOCKER_LENGTH 1024

static char blocker_list[MAX_BLOCKER_LENGTH];
static DEFINE_SPINLOCK(wakelock_blocker_lock);

static bool name_in_list(const char *list, const char *name)
{
	size_t nlen = strlen(name);
	const char *p = list;

	if (!nlen)
		return false;

	while (*p) {
		while (*p == ',')
			p++;
		if (!*p)
			break;
		if (!strncmp(p, name, nlen) &&
		    (p[nlen] == ',' || p[nlen] == '\0'))
			return true;
		p = strchr(p, ',');
		if (!p)
			break;
	}
	return false;
}

static void name_add_to_list(char *list, size_t size, const char *name)
{
	size_t llen, nlen;

	if (name_in_list(list, name))
		return;

	nlen = strlen(name);
	llen = strlen(list);
	if (llen + nlen + 2 > size)
		return;
	if (llen)
		list[llen++] = ',';
	strncpy(list + llen, name, size - llen);
	list[size - 1] = '\0';
}

static void name_remove_from_list(char *list, size_t size, const char *name)
{
	char *start, *end;
	size_t nlen = strlen(name);

	if (!nlen)
		return;

	start = list;
	while (start < list + size && *start) {
		while (*start == ',')
			start++;
		if (!*start)
			break;
		end = strchr(start, ',');
		if (!end)
			end = list + strlen(list);
		if ((size_t)(end - start) == nlen &&
		    !strncmp(start, name, nlen)) {
			size_t rest = strlen(end);

			memmove(start, end, rest + 1);
			if (start > list)
				start--;
			continue;
		}
		start = end + 1;
	}
}

bool wakelock_blocker_is_blocked(const char *name)
{
	unsigned long flags;
	bool blocked;

	if (!name || !name[0])
		return false;

	spin_lock_irqsave(&wakelock_blocker_lock, flags);
	blocked = name_in_list(blocker_list, name);
	spin_unlock_irqrestore(&wakelock_blocker_lock, flags);

	return blocked;
}
EXPORT_SYMBOL_GPL(wakelock_blocker_is_blocked);

static ssize_t wakelock_blocker_read(struct file *file, char __user *buf,
				     size_t count, loff_t *offset)
{
	unsigned long flags;
	ssize_t ret;

	spin_lock_irqsave(&wakelock_blocker_lock, flags);
	ret = simple_read_from_buffer(buf, count, offset, blocker_list,
				      strlen(blocker_list));
	spin_unlock_irqrestore(&wakelock_blocker_lock, flags);

	return ret;
}

static ssize_t wakelock_blocker_write(struct file *file, const char __user *buf,
				      size_t count, loff_t *offset)
{
	char *buffer, *entry, *saveptr;
	unsigned long flags;
	int ret = 0;

	if (count >= MAX_BLOCKER_LENGTH)
		return -EINVAL;

	buffer = kzalloc(count + 1, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	if (copy_from_user(buffer, buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	spin_lock_irqsave(&wakelock_blocker_lock, flags);
	for (entry = strtok_r(buffer, ",", &saveptr); entry;
	     entry = strtok_r(NULL, ",", &saveptr)) {
		while (*entry == '\n' || *entry == ' ')
			entry++;
		if (*entry == '!')
			name_remove_from_list(blocker_list, MAX_BLOCKER_LENGTH,
					      entry + 1);
		else
			name_add_to_list(blocker_list, MAX_BLOCKER_LENGTH,
					 entry);
	}
	spin_unlock_irqrestore(&wakelock_blocker_lock, flags);

out:
	kfree(buffer);
	return ret ? ret : count;
}

static const struct file_operations wakelock_blocker_fops = {
	.owner		= THIS_MODULE,
	.read		= wakelock_blocker_read,
	.write		= wakelock_blocker_write,
};

static struct miscdevice wakelock_blocker_misc = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "wakelock_blocker",
	.fops		= &wakelock_blocker_fops,
};

static int __init wakelock_blocker_init(void)
{
	return misc_register(&wakelock_blocker_misc);
}

static void __exit wakelock_blocker_exit(void)
{
	misc_deregister(&wakelock_blocker_misc);
}

module_init(wakelock_blocker_init);
module_exit(wakelock_blocker_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Wakelock blocker driver");

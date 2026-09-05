/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("mrThinBone"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static ssize_t append_unfinished_entry(struct aesd_dev *dev, const char __user *buf, size_t count)
{
    ssize_t retval = -ENOMEM;
    struct aesd_buffer_entry *unfinished_entry = dev->unfinished_entry;
    if (unfinished_entry == NULL) {
        unfinished_entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
        if (unfinished_entry == NULL) {
            goto out;
        }
        unfinished_entry->buffptr = kmalloc(count, GFP_KERNEL);
        if (unfinished_entry->buffptr == NULL) {
            kfree(unfinished_entry);
            goto out;
        }
        unfinished_entry->size = 0;
    } else {
        char *new_buffptr = krealloc(unfinished_entry->buffptr, unfinished_entry->size + count, GFP_KERNEL);
        if (new_buffptr == NULL) {
            goto out;
        }
        unfinished_entry->buffptr = new_buffptr;
    }

    if (copy_from_user(unfinished_entry->buffptr + unfinished_entry->size, buf, count)) {
        retval = -EFAULT;
        goto out;
    }
    unfinished_entry->size += count;
    dev->unfinished_entry = unfinished_entry;
    retval = count;
    out:
        return retval;
}

static ssize_t circular_buffer_add_entry(struct aesd_circular_buffer *circ_buf, const struct aesd_buffer_entry *add_entry)
{
    ssize_t retval = 0;
    if (circ_buf == NULL || add_entry == NULL) {
        retval = -EINVAL;
        goto out;
    }
    char full = circ_buf->full;
    int in_offs = circ_buf->in_offs;
    // free up the memory of the entry being overwritten
    if (full == 1) {
        kfree(circ_buf->entry[in_offs].buffptr);
        circ_buf->entry[in_offs].buffptr = NULL;
        circ_buf->entry[in_offs].size = 0;
    }
    // write the new entry to the circular buffer
    circ_buf->entry[in_offs] = *add_entry;
    
    char first_reach = 0;
    if (in_offs + 1 >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
        circ_buf->in_offs = 0;
        if (full == 0) {
            first_reach = 1;
        }
    } else {
        circ_buf->in_offs++;
    }

    if (first_reach) {
        circ_buf->full = 1;
    } else {
        if (full == 1) {
            circ_buf->out_offs++;
            if (circ_buf->out_offs >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
                circ_buf->out_offs = 0;
            }
        }
    }
    retval = add_entry->size;
    out:
        return retval;
}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0; // end of file indicator
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev = filp->private_data;
    struct aesd_circular_buffer *circ_buf = dev->circular_buffer;
    
    if (circ_buf == NULL) {
        return -EINVAL;
    }

    if (down_read_interruptible(&dev->lock) != 0) {
        return -ERESTARTSYS;
    }

    int out_offs = circ_buf->out_offs;
    ssize_t offset = 0;
    int i = 1;
    do {
        if (circ_buf->entry[out_offs].buffptr != NULL) {
            if (offset + circ_buf->entry[out_offs].size > *f_pos) {
                // calculate the offset within the entry to read from
                // "hello\n" -> entry[0], size=6
                // *f_pos = 2, offset = 0, entry_offset = 2
                size_t entry_offset = *f_pos - offset;
                // we'll return "llo\n" to the user, which is 4 bytes (6-2)
                retval = circ_buf->entry[out_offs].size - entry_offset;
                if (retval > count) {
                    retval = count;
                }
                if (copy_to_user(buf, circ_buf->entry[out_offs].buffptr + entry_offset, retval)) {
                    retval = -EFAULT;
                    goto out;
                }
                // seek to the next position after the read data
                // *f_pos = 6
                *f_pos += retval;
                goto out;
            }
            offset += circ_buf->entry[out_offs].size;
            out_offs++;
            if (out_offs >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
                out_offs = 0;
            }
        } else {
            break;
        }
    } while (++i <= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);

    out:
        up_read(&dev->lock);
        return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    if (count == 0) {
        return 0;
    }
    /**
     * TODO: handle write
     */
    struct aesd_dev *dev = filp->private_data;
    struct aesd_circular_buffer *circ_buf = dev->circular_buffer;
    
    if (circ_buf == NULL) {
        return -EINVAL;
    }

    char last_char;
    if (copy_from_user(&last_char, buf + count - 1, 1)) {
        return -EFAULT;
    }

    if (down_write_killable(&dev->lock) != 0) {
        return -ERESTARTSYS;
    }
    
    if (last_char != '\n') {
        PDEBUG(">>> last char is not newline <<<");
        // keep appending to the unfinished entry until we get a newline
        ssize_t retval_append = append_unfinished_entry(dev, buf, count);
        if (retval_append < 0) {
            retval = retval_append;
            goto out;
        }
        retval = count;
        goto out;
    }
    PDEBUG(">>> last char is newline <<<");
    PDEBUG(">>> write to circular buffer: %ld <<<", count);

    struct aesd_buffer_entry *unfinished_entry = dev->unfinished_entry;
    if (unfinished_entry != NULL) {
        PDEBUG(">>> unfinished entry exists <<<");
        ssize_t retval_append = append_unfinished_entry(dev, buf, count);
        if (retval_append < 0) {
            retval = retval_append;
            goto out;
        }
        struct aesd_buffer_entry new_entry = *unfinished_entry;
        retval = circular_buffer_add_entry(circ_buf, &new_entry);
        if (retval >= 0) {
            retval = count;
        }
        // buffptr ownership is tranffered to circular-buffer
        // free only the wrapper
        kfree(unfinished_entry);
        dev->unfinished_entry = NULL;
    } else { 
        struct aesd_buffer_entry new_entry;
        new_entry.buffptr = kmalloc(count, GFP_KERNEL);
        if (new_entry.buffptr == NULL) {
            retval = -ENOMEM;
            goto out;
        }
        if (copy_from_user(new_entry.buffptr, buf, count)) {
            kfree(new_entry.buffptr);
            retval = -EFAULT;
            goto out;
        }
        new_entry.size = count;
        retval = circular_buffer_add_entry(circ_buf, &new_entry);
    }

    out:
        up_write(&dev->lock);
        return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_device.circular_buffer = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    if (aesd_device.circular_buffer == NULL) {
        result = -ENOMEM;
        unregister_chrdev_region(dev, 1);
        return result;
    }
    memset(aesd_device.circular_buffer, 0, sizeof(struct aesd_circular_buffer));

    aesd_device.circular_buffer->entry = kmalloc(sizeof(struct aesd_buffer_entry) * AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED, GFP_KERNEL);
    if (aesd_device.circular_buffer->entry == NULL) {
        kfree(aesd_device.circular_buffer);
        aesd_device.circular_buffer = NULL;
        result = -ENOMEM;
        unregister_chrdev_region(dev, 1);
        return result;
    }
    memset(aesd_device.circular_buffer->entry, 0, sizeof(struct aesd_buffer_entry) * AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);

    init_rwsem(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific portions here as necessary
     */
    if (aesd_device.circular_buffer != NULL) {
        for (int i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
            if (aesd_device.circular_buffer->entry[i].buffptr != NULL) {
                kfree(aesd_device.circular_buffer->entry[i].buffptr);
                aesd_device.circular_buffer->entry[i].buffptr = NULL;
                aesd_device.circular_buffer->entry[i].size = 0;
            }
        }
        kfree(aesd_device.circular_buffer->entry);
        aesd_device.circular_buffer->entry = NULL;
        kfree(aesd_device.circular_buffer);
        aesd_device.circular_buffer = NULL;
    }

    // mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

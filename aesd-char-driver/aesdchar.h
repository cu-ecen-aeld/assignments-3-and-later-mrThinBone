/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#define AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED 10

struct aesd_buffer_entry {
     void* buffptr;
     ssize_t size;
};

struct aesd_circular_buffer {
    struct aesd_buffer_entry* entry;
    int in_offs;
    int out_offs;
    char full;
};

struct aesd_dev
{
    /**
     * TODO: Add structure(s) and locks needed to complete assignment requirements
     */
    struct aesd_circular_buffer* circular_buffer;
    struct aesd_buffer_entry* unfinished_entry;
    struct rw_semaphore lock;
    struct cdev cdev;     /* Char device structure      */
};


#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */

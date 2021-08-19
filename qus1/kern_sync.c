/*1. Kernel Synchronization Mechanisms for Multiple devices.
a) Write a character driver for Multiple devices and register 2 device numbers in
the kernel space and implements Read and Write Functionality in the kernel
space.
b) Maintain a global Kernel buffer of 50 bytes which will be used by the Read and
Write methods of the driver. Use a semaphore to protect the buffer which
represents a Critical section.
c) Also, use wait-queues to avoid consecutive write operations on the 2 devices that
are created to prevent the overwrite of the previous data.
d) Write 2 separate user space programs that operate on the 2 devices, individually.
The write function in the user application should be followed by sleep and then a
read. However, the write should put 80 bytes of data, while the read should read
it from the kernel.
e) Since the write function is trying to write 80 bytes, the write function should be
made to sleep using wait-queues until read is completed. The read function
should Wake-up the sleeping write function to copy remaining data. */

#include<linux/module.h>
#include <linux/kernel.h> 
#include <linux/fs.h>	
#include <linux/cdev.h> 	
#include <linux/semaphore.h> 
#include <linux/uaccess.h>  	
#include<linux/kdev_t.h>
#include<linux/wait.h>
#include<linux/sched.h>

wait_queue_head_t sample_waitq;

struct fake_device
{
	char data[50];
	struct semaphore sem;
} virtual_device;


struct cdev *mcdev; 
struct cdev *mcdev2;
int major_number,minor_number; 

int ret,flag=0;			
dev_t dev_num;

#define DEVICE_NAME "kern_sync"


static int device_open(struct inode *inode, struct file *filp) //   open--------------------------------
{
		

	printk(KERN_INFO "kern_sync : -- device opened ");
	return 0;
}
	
ssize_t device_read(struct file* filp, char* bufStoreData, size_t bufCount , loff_t* curoffset)
	{
	
	if(down_interruptible(&virtual_device.sem) != 0)//****************************
	{	printk(KERN_ALERT "kern_sync : could not lock device during open");
		return -1;
	}
	printk(KERN_INFO "kern_sync: reading from device ");	
	
			
			if(wait_event_interruptible(sample_waitq, flag == 1))
				return -ERESTARTSYS;

	
	ret = copy_to_user (bufStoreData,virtual_device.data,bufCount);
	
	
	flag = 0;
	
	up(&virtual_device.sem);			//********************************
	return ret;
	}
	
	
ssize_t device_write(struct file* filp, const char* bufSourceData , size_t bufCount, loff_t* curoffset)
	{
	
	if(down_interruptible(&virtual_device.sem) != 0)//*************************
	{	printk(KERN_ALERT "kern_sync : could not lock device during open");
		return -1;
	}
	printk(KERN_INFO "kern_sync: writing to device ");
	ret = copy_from_user(virtual_device.data,bufSourceData,bufCount);
	
			
	
			flag = 1;

			
			wake_up_interruptible(&sample_waitq);
			
	up(&virtual_device.sem);//**********************************
	return ret;	
	}
	


static int device_close(struct inode *inode, struct file *filp)

	
	printk(KERN_INFO "kern_sync: closed device");
	return 0;
}


struct file_operations fops = {
	.owner = THIS_MODULE, 
	.open = &device_open, 
	.release = &device_close, 
	.read = device_read,
	.write = device_write,
	};
static int driver_entry(void)
{
	ret = alloc_chrdev_region(&dev_num,1,2,DEVICE_NAME);
	if(ret < 0)
	{	
		printk(KERN_INFO "kern_sync is failed to allocate major number\n");
		return ret;
	}

		major_number = MAJOR (dev_num); 
		minor_number = MINOR (dev_num); 
		printk(KERN_INFO "kern_sync : najor number is %d  & %d",major_number,minor_number);
		printk(KERN_INFO "\t use \"mknod /dev/%s  c %d  0 \" for device_file",DEVICE_NAME,major_number);
		
		//step2 
		mcdev = cdev_alloc();	
		mcdev->ops = &fops;
		mcdev->owner = THIS_MODULE;
						
		ret = cdev_add(mcdev, dev_num,1);
		if(ret < 0)
		{
			printk(KERN_ALERT "kern_sync: unable to add cdev to kernel \n");
			return ret;
		}
		//step ----------------
		sema_init(&virtual_device.sem,1);
		
					
				init_waitqueue_head(&sample_waitq);

		

return 0;
}

	

static void driver_exit(void)	
{
	cdev_del(mcdev); 
	unregister_chrdev_region(dev_num, 1);
	printk(KERN_ALERT "kern_sync: unload module "); 
}

module_init(driver_entry);
module_exit(driver_exit);

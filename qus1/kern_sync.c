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
#include <linux/fs.h>		// file operations structure- which of course allows use to open/close, read/write to device
#include <linux/cdev.h> 	//this is a char driver; makes cdev available 
#include <linux/semaphore.h> 
#include <linux/uaccess.h>  	//copy_to_user;copy_from_user
#include<linux/kdev_t.h>
#include<linux/wait.h>
#include<linux/sched.h>

/* Declare a variable of type 'wait_queue_head_t' to perform the operations */
wait_queue_head_t sample_waitq;

//(1) creating a structure for our fake device
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
	//(8)called when user wants to get info from from the device.
ssize_t device_read(struct file* filp, char* bufStoreData, size_t bufCount , loff_t* curoffset)
	{
	//take data from kernel space(device)  to  userspace (process)
	//copy_to_user(destination , source , sizeto transfer);
	
	if(down_interruptible(&virtual_device.sem) != 0)//****************************
	{	printk(KERN_ALERT "kern_sync : could not lock device during open");
		return -1;
	}
	printk(KERN_INFO "kern_sync: reading from device ");	
	
			/* Push the process into waitqueue if data is not available, i.e., flag == 0  */
			if(wait_event_interruptible(sample_waitq, flag == 1))
				return -ERESTARTSYS;

	
	ret = copy_to_user (bufStoreData,virtual_device.data,bufCount);
	
	/* Clear the flag indicating that no data is available */
	flag = 0;
	
	up(&virtual_device.sem);			//********************************
	return ret;
	}
	
	//(9) called when user wants to send info to the device	
ssize_t device_write(struct file* filp, const char* bufSourceData , size_t bufCount, loff_t* curoffset)
	{
	//send data from user to kernel
	//copy_from_user (dest, source, count)
	if(down_interruptible(&virtual_device.sem) != 0)//*************************
	{	printk(KERN_ALERT "kern_sync : could not lock device during open");
		return -1;
	}
	printk(KERN_INFO "kern_sync: writing to device ");
	ret = copy_from_user(virtual_device.data,bufSourceData,bufCount);
	
			/* Set the flag value to assure that data is now available */
			flag = 1;

			/* Wake up the process waiting for the data */
			wake_up_interruptible(&sample_waitq);
			
	up(&virtual_device.sem);//**********************************
	return ret;	
	}
	
	





	//(10) called upon user close
static int device_close(struct inode *inode, struct file *filp)
{
	//by calling up, which is opposite of down for semaphore , we release the mutex that we obtained at device open
	//this allows other process to use the device now
	
	printk(KERN_INFO "kern_sync: closed device");
	return 0;
}


//(11) tell the kernel which function to call when user operates on our device  file
struct file_operations fops = {
	.owner = THIS_MODULE, //prevent unloading of this module when operations are in use
	.open = &device_open, //points to the method to call when opening the device
	.release = &device_close,//points to the method to call when closing the device 
	.read = device_read,//points to method to call when reading from device
	.write = device_write,//points to method to call when writing to device
	};
static int driver_entry(void)
{
	//(3)here Register our device with system: a 2step process
	//step1: using dynamic allocation to assign our device
	//a major num--alloc chrdev_region(dev_t*, uint fminor, uint count, char* name)
	
	ret = alloc_chrdev_region(&dev_num,1,2,DEVICE_NAME);//dev_num,minor,no_of devices,devicename
	if(ret < 0)
	{	//alloc returns negative number if there is an error
		printk(KERN_INFO "kern_sync is failed to allocate major number\n");
		return ret;
	}

		major_number = MAJOR (dev_num); //extracts mjr num stored in dev_num structure
		minor_number = MINOR (dev_num); 
		printk(KERN_INFO "kern_sync : najor number is %d  & %d",major_number,minor_number);
		printk(KERN_INFO "\t use \"mknod /dev/%s  c %d  0 \" for device_file",DEVICE_NAME,major_number);
		
		//step2 
		mcdev = cdev_alloc();	//create our cdev structure , initialized our cdev
		mcdev->ops = &fops;	// it is structure in which we define function calls
		mcdev->owner = THIS_MODULE;
						// now that we created cdev, we have to add it to the kernel
						// int cdev_add(struct cdev* dev, dev_t num,unsigned int count )
		ret = cdev_add(mcdev, dev_num,1);
		if(ret < 0)
		{
			printk(KERN_ALERT "kern_sync: unable to add cdev to kernel \n");
			return ret;
		}
		//step ----------------
		sema_init(&virtual_device.sem,1); //inital value of one
		
					/* Initialise the waitqueue */
				init_waitqueue_head(&sample_waitq);

		

return 0;
}

	

static void driver_exit(void)	//step(4) exit module, in this unregister everything yove registerd 
{
	cdev_del(mcdev); //unregistering everything in reverse order.
	unregister_chrdev_region(dev_num, 1);
	printk(KERN_ALERT "kern_sync: unload module ");
}

module_init(driver_entry);
module_exit(driver_exit);

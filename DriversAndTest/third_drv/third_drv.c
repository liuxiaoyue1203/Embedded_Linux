#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>


static struct class *thirddrv_class;
static struct class_device	*thirddrv_class_dev;

//volatile unsigned long *gpfcon;
//volatile unsigned long *gpfdat;

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

/* �ж��¼���־, �жϷ����������1��third_drv_read������0 */
static volatile int ev_press = 0;


struct pin_desc{
	unsigned int pin;
	unsigned int key_val;
};


/* ��ֵ: ����ʱ, 0x01, 0x02, 0x03, 0x04 */
/* ��ֵ: �ɿ�ʱ, 0x81, 0x82, 0x83, 0x84 */
static unsigned char key_val;

/*
 * K1,K2,K3,K4��ӦGPG0��GPG3��GPG5��GPG6
 */

struct pin_desc pins_desc[4] = {
	{S3C2410_GPG0, 0x01},
	{S3C2410_GPG3, 0x02},
	{S3C2410_GPG5, 0x03},
	{S3C2410_GPG6, 0x04},
};


/*
  * ȷ������ֵ
  */
static irqreturn_t buttons_irq(int irq, void *dev_id)
{
	struct pin_desc * pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;
	
	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if (pinval)
	{
		/* �ɿ� */
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		/* ���� */
		key_val = pindesc->key_val;
	}

    ev_press = 1;                  /* ��ʾ�жϷ����� */
    wake_up_interruptible(&button_waitq);   /* �������ߵĽ��� */

	
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int third_drv_open(struct inode *inode, struct file *file)
{
	/* GPG0��GPG3��GPG5��GPG6Ϊ�ж�����: EINT8,EINT11,EINT13,EINT14 */
	request_irq(IRQ_EINT8,  buttons_irq, IRQT_BOTHEDGE, "K1", &pins_desc[0]);
	request_irq(IRQ_EINT11, buttons_irq, IRQT_BOTHEDGE, "K2", &pins_desc[1]);
	request_irq(IRQ_EINT13, buttons_irq, IRQT_BOTHEDGE, "K3", &pins_desc[2]);
	request_irq(IRQ_EINT14, buttons_irq, IRQT_BOTHEDGE, "K4", &pins_desc[3]);	

	return 0;
}

ssize_t third_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	if (size != 1)
		return -EINVAL;

	/* ���û�а�������, ���� */
	wait_event_interruptible(button_waitq, ev_press);

	/* ����а�������, ���ؼ�ֵ */
	copy_to_user(buf, &key_val, 1);
	ev_press = 0;
	
	return 1;
}


int third_drv_close(struct inode *inode, struct file *file)
{
	free_irq(IRQ_EINT8,  &pins_desc[0]);
	free_irq(IRQ_EINT11, &pins_desc[1]);
	free_irq(IRQ_EINT13, &pins_desc[2]);
	free_irq(IRQ_EINT14, &pins_desc[3]);
	return 0;
}


static struct file_operations sencod_drv_fops = {
    .owner   =  THIS_MODULE,    /* ����һ���꣬�������ģ��ʱ�Զ�������__this_module���� */
    .open    =  third_drv_open,     
	.read	 =	third_drv_read,	   
	.release =  third_drv_close,	   
};


int major;
static int third_drv_init(void)
{
	major = register_chrdev(0, "third_drv", &sencod_drv_fops);

	thirddrv_class = class_create(THIS_MODULE, "third_drv");

	thirddrv_class_dev = class_device_create(thirddrv_class, NULL, MKDEV(major, 0), NULL, "buttons"); /* /dev/buttons */

//	gpfcon = (volatile unsigned long *)ioremap(0x56000050, 16);
//	gpfdat = gpfcon + 1;

	return 0;
}

static void third_drv_exit(void)
{
	unregister_chrdev(major, "third_drv");
	class_device_unregister(thirddrv_class_dev);
	class_destroy(thirddrv_class);
//	iounmap(gpfcon);
	return 0;
}


module_init(third_drv_init);

module_exit(third_drv_exit);

MODULE_LICENSE("GPL");

/* 
 * $Log: as_misc_watchdog.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
 * rebuild
 *
 * Revision 1.3  2005/07/07 03:01:27  wangwei
 * 关闭看门狗打印信息
 *
 * Revision 1.2  2005/06/17 09:05:10  wangwei
 * no message
 *
 * Revision 1.1  2005/06/07 09:16:21  lizhijie
 * add into CVS
 *
 * $Id: as_misc_watchdog.c,v 1.1.1.1 2006/11/29 09:16:54 lizhijie Exp $
 * driver for IXP4xx WatchDog chip
 * as_misc_watchdog.c : Hardware manipulate operation
 * Li Zhijie, 2005.06.07
*/

#include <linux/config.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/interrupt.h>
#include <linux/time.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/timex.h>
#include <asm/hardware.h>

#include "as_misc_dev.h"

#define WATCHDOG_DEBUG								0

#define AS_IXP_WATCHDOG_COUNT_ENABLE				BIT(2)
#define AS_IXP_WATCHDOG_INTERRUPT_ENABLE			BIT(1)
#define AS_IXP_WATCHDOG_WARM_RESET_ENABLE			BIT(0)

#define AS_IXP_WATCHDOG_KEY_VALUE					0x482E

typedef struct
{
	unsigned int count_value;		/* total count */

	int status;						/* value in status register */
	int config;						/* value in watchdog enable register */
	unsigned int current_count;		/* current count in register */
}AS_WATCHDOG_STATUS;

static AS_WATCHDOG_STATUS watchdog_status;


static void as_ixp_watchdog_read_reg(void )
{
	watchdog_status.current_count = *IXP425_OSWT;
	watchdog_status.config = *IXP425_OSWE;
	watchdog_status.status =  *IXP425_OSST;
}

int as_watchdog_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	as_misc_dev	*dev;

	if (off > 0)
		return 0;
		
	dev = (as_misc_dev *)data;
	if (!dev)
		return 0;

	MOD_INC_USE_COUNT;

	as_ixp_watchdog_read_reg();
	
	if ( dev->name ) 
	{
		len += sprintf(page + len, "%s : ", dev->name);
//		len += sprintf(page + len,  AS_VERSION_INFO("WatchDog") );
	}

	len += sprintf(page + len, "\r\n\r\n");
	len += sprintf(page + len, "Count Period : %d ms \tLeft : %d ms\r\n", watchdog_status.count_value/1000/1000*15, watchdog_status.current_count/1000/1000*15 );

	len += sprintf(page + len, "WatchDog Config : \r\n" );
	len += sprintf(page + len, "\tCount \t\t: %s \r\n",  (watchdog_status.config&AS_IXP_WATCHDOG_COUNT_ENABLE)? "Enable":"Disbale" );
	len += sprintf(page + len, "\tInterrupt \t: %s \r\n",  (watchdog_status.config&AS_IXP_WATCHDOG_INTERRUPT_ENABLE)? "Enable":"Disbale" );
	len += sprintf(page + len, "\tWarm Reset \t: %s \r\n", (watchdog_status.config&AS_IXP_WATCHDOG_WARM_RESET_ENABLE)? "Enable":"Disbale" );

	MOD_DEC_USE_COUNT;
	
	return len;
}


static void  as_ixp_watchdog_enable(void )
{
	/* enable watchdog register (count and enable) write operation */
	*IXP425_OSWK = AS_IXP_WATCHDOG_KEY_VALUE;
	/* set count value , max is 0XFFFFFFFE,  in the unit of 15 ns */
	*IXP425_OSWT = watchdog_status.count_value; 
	/* enable count/interrupt/warm-reset of watchdog chip */
	// *IXP425_OSWE = AS_IXP_WATCHDOG_COUNT_ENABLE|AS_IXP_WATCHDOG_INTERRUPT_ENABLE |AS_IXP_WATCHDOG_WARM_RESET_ENABLE;
	*IXP425_OSWE = AS_IXP_WATCHDOG_COUNT_ENABLE|AS_IXP_WATCHDOG_WARM_RESET_ENABLE;

#if WATCHDOG_DEBUG
	printk(KERN_INFO "Assist WatchDog enabled\r\n");
#endif
	/* disable watchdog register (count and enable) write operation */
//	*IXP425_OSWK = 0;
}


void as_ixp_watchdog_disable(void )
{
	/* enable watchdog register (count and enable) write operation */
	*IXP425_OSWK = AS_IXP_WATCHDOG_KEY_VALUE;
	/* restore the default value */
	*IXP425_OSWT = AS_IXP_WATCHDOG_DEFAULT_COUNT;
	/* disable count/interrupt/warm-reset of watchdog chip */
	*IXP425_OSWE = 0;

	/* disable watchdog register (count and enable) write operation */
	*IXP425_OSWK = 0;
#if WATCHDOG_DEBUG
	printk(KERN_INFO "Assist WatchDog disabled\r\n");
#endif
}

static void as_ixp_watchdog_update(void )
{
	/* enable watchdog register (count and enable) write operation */
//	*IXP425_OSWK = AS_IXP_WATCHDOG_KEY_VALUE;
	/* set count value */
	*IXP425_OSWT = watchdog_status.count_value;

//	*IXP425_OSWK = 0;
#if WATCHDOG_DEBUG
	//printk(KERN_INFO "Assist WatchDog updated\r\n");
#endif
}


int as_watchdog_proc_write(struct file *file, const char *buf, unsigned long count, void *data)
{
	AS_WATCHDOG_COMMAND cmd;
	int len;
	

	MOD_INC_USE_COUNT;

	len = copy_from_user(&cmd, buf, sizeof(AS_WATCHDOG_COMMAND ));
	if( len )
	{
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	switch(cmd.type)
	{
		case AS_WATCHDOG_ENABLE:
			watchdog_status.count_value = cmd.value;
			/* some condition check here */
			as_ixp_watchdog_enable();
			break;
			
		case AS_WATCHDOG_DISABLE:
			watchdog_status.count_value = AS_IXP_WATCHDOG_DEFAULT_COUNT;
			as_ixp_watchdog_disable();
			break;
			
		case AS_WATCHDOG_UPDATE:
			as_ixp_watchdog_update();
			break;
			
		default:
			printk(KERN_INFO "Not validate command for Assist WatchDog Driver\r\n");
			len = 0;
			break;
	}
	
	MOD_DEC_USE_COUNT;

	return len;
}


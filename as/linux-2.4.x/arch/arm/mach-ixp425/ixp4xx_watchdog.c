/*
* ixp425_watchdog.c
* driver for IXP4xx watchdog timer chip, Li Zhijie,2005.05.27
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

#define WATCHDOG_DEBUG		1

#define AS_IXP_WATCHDOG_COUNT_ENABLE				BIT(2)
#define AS_IXP_WATCHDOG_INTERRUPT_ENABLE			BIT(1)
#define AS_IXP_WATCHDOG_WARM_RESET_ENABLE			BIT(0)

#define  AS_IXP_WATCHDOG_KEY_VALUE					0x482E

void __init as_ixp_watchdog_setup()
{
	/* enable watchdog register (count and enable) write operation */
	*IXP425_OSWK = AS_IXP_WATCHDOG_KEY_VALUE;
	/* set count value */
	*IXP425_OSWT = 0XFFFFFFFE;
	/* enable count/interrupt/warm-reset of watchdog chip */
	// *IXP425_OSWE = AS_IXP_WATCHDOG_COUNT_ENABLE|AS_IXP_WATCHDOG_INTERRUPT_ENABLE |AS_IXP_WATCHDOG_WARM_RESET_ENABLE;
	*IXP425_OSWE = AS_IXP_WATCHDOG_COUNT_ENABLE|AS_IXP_WATCHDOG_WARM_RESET_ENABLE;

	/* disable watchdog register (count and enable) write operation */
//	*IXP425_OSWK = 0;
}

#if 0
static void as_ixp_watchdog_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{

#if WATCHDOG_DEBUG
	printk(KERN_INFO "Watchdog Interrupt : %d\r\n", int_count);
#endif

	/* Clear Pending Interrupt (Warm-reset and Watchdog interrupt) by writing '1' to it */
	*IXP425_OSST = IXP425_OSST_TIMER_WARM_RESET |IXP425_OSST_TIMER_WDOG_PEND;

	/* enable watchdog register (count and enable) write operation */
	*IXP425_OSWK = AS_IXP_WATCHDOG_KEY_VALUE;

	/* set count value, about 64.4 seconds */
	*IXP425_OSWT = 0XFFFFFFFE;

	/* disable watchdog register (count and enable) write operation */
	*IXP425_OSWK = 0;
}

static struct irqaction watch_irq = 
{
	name: "WatchDog Timer",
	flags: SA_INTERRUPT
};

void __init as_ixp_watchdog_setup_timer(void)
{
	watch_irq.handler = as_ixp_watchdog_interrupt;

#if WATCHDOG_DEBUG
	printk(KERN_INFO "IXP4xx Watchdog timer chip initialized\r\n");
#endif
	as_ixp_watchdog_setup();
	/* Clear Pending Interrupt by writing '1' to it */
	*IXP425_OSST = IXP425_OSST_TIMER_1_PEND;

	/* Connect the interrupt handler and enable the interrupt */
	setup_arm_irq(IRQ_IXP425_WDOG, &watch_irq);
}
#endif

#ifdef MODULE
int as_ixp_watchdog_init(void)
{
	as_ixp_watchdog_setup_timer();
	return 0;
}

void as_ixp_watchdog_exit(void)
{
	*IXP425_OSWK = AS_IXP_WATCHDOG_KEY_VALUE;
	/* set count value */
	*IXP425_OSWT = 0XFFFFFFFE;
	/* disable count/interrupt/warm-reset of watchdog chip */
	*IXP425_OSWE = 0;
	/* disable watchdog register (count and enable) write operation */
	*IXP425_OSWK = 0;

  	printk(KERN_INFO "IXP4XX Watchdog driver unload\r\n");
	return ;
}

module_init(as_ixp_watchdog_init);
module_exit(as_ixp_watchdog_exit);

#endif


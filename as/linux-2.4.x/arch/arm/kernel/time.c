/*
 *  linux/arch/arm/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *  Modifications for ARM (C) 1994, 1995, 1996,1997 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This file contains the ARM-specific time handling details:
 *  reading the RTC at bootup, etc...
 *
 *  1994-07-02  Alan Modra
 *              fixed set_rtc_mmss, fixed time.year for >= 2000, new mktime
 *  1998-12-20  Updated NTP code according to technical memorandum Jan '96
 *              "A Kernel Model for Precision Timekeeping" by Dave Mills
 $Author: lizhijie $
$Log: time.c,v $
Revision 1.1.1.1  2006/11/29 08:55:09  lizhijie
AS600 Kernel

Revision 1.1.1.1  2005/07/08 09:46:05  lizhijie
Linux-2.4.24 integrated Intel Access Library 2.0

Revision 1.4  2005/04/19 08:02:53  wangwei
修改了在编译过程中警告的地方

Revision 1.3  2005/04/18 07:48:21  wangwei
no message

$Revision: 1.1.1.1 $
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/timex.h>
#include <asm/hardware.h>

extern void __init setup_timer(void);
extern void rtc_init_startup();
extern unsigned long get_cmos_time(void);
extern int setup_arm_irq(int, struct irqaction *);
extern rwlock_t xtime_lock;
extern unsigned long wall_jiffies;

/* change this if you have some constant time drift */
#define USECS_PER_JIFFY	(1000000/HZ)

#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif

static int dummy_set_rtc(void)
{
	return 0;
}

/*
 * hook for setting the RTC's idea of the current time.
 */
int (*set_rtc)(void) = dummy_set_rtc;

static unsigned long dummy_gettimeoffset(void)
{
	return 0;
}

/*
 * hook for getting the time offset.  Note that it is
 * always called with interrupts disabled.
 */
unsigned long (*gettimeoffset)(void) = dummy_gettimeoffset;

/*
 * Handle kernel profile stuff...
 */
static inline void do_profile(struct pt_regs *regs)
{
	if (!user_mode(regs) &&
	    prof_buffer &&
	    current->pid) {
		unsigned long pc = instruction_pointer(regs);
		extern int _stext;

		pc -= (unsigned long)&_stext;

		pc >>= prof_shift;

		if (pc >= prof_len)
			pc = prof_len - 1;

		prof_buffer[pc] += 1;
	}
}

static long next_rtc_update;

/*
 * If we have an externally synchronized linux clock, then update
 * CMOS clock accordingly every ~11 minutes.  set_rtc() has to be
 * called as close as possible to 500 ms before the new second
 * starts.
 */
static inline void do_set_rtc(void)
{
	if (time_status & STA_UNSYNC || set_rtc == NULL)
		return;

	if (next_rtc_update &&
	    time_before(xtime.tv_sec, next_rtc_update))
		return;

	if (xtime.tv_usec < 50000 - (tick >> 1) &&
	    xtime.tv_usec >= 50000 + (tick >> 1))
		return;

	if (set_rtc())
		/*
		 * rtc update failed.  Try again in 60s
		 */
		next_rtc_update = xtime.tv_sec + 60;
	else
		next_rtc_update = xtime.tv_sec + 660;
}

#ifdef CONFIG_LEDS

#include <asm/leds.h>

static void dummy_leds_event(led_event_t evt)
{
}

void (*leds_event)(led_event_t) = dummy_leds_event;

#ifdef CONFIG_MODULES
EXPORT_SYMBOL(leds_event);
#endif
#endif

#ifdef CONFIG_LEDS_TIMER
void do_leds(void)
{
	static unsigned int count = 50;

	if (--count == 0) {
		count = 50;
		leds_event(led_timer);
	}
}
#else
#define do_leds()
#endif

void do_gettimeofday(struct timeval *tv)
{
	unsigned long flags;
	unsigned long usec, sec;

	read_lock_irqsave(&xtime_lock, flags);
	usec = gettimeoffset();
	{
		unsigned long lost = jiffies - wall_jiffies;

		if (lost)
			usec += lost * USECS_PER_JIFFY;
	}
	sec = xtime.tv_sec;
	usec += xtime.tv_usec;
	read_unlock_irqrestore(&xtime_lock, flags);

	/* usec may have gone up a lot: be safe */
	while (usec >= 1000000) {
		usec -= 1000000;
		sec++;
	}

	tv->tv_sec = sec;
	tv->tv_usec = usec;
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq(&xtime_lock);
	/* This is revolting. We need to set the xtime.tv_usec
	 * correctly. However, the value in this location is
	 * is value at the last tick.
	 * Discover what correction gettimeofday
	 * would have done, and then undo it!
	 */
	tv->tv_usec -= gettimeoffset();
	tv->tv_usec -= (jiffies - wall_jiffies) * USECS_PER_JIFFY;

	while (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irq(&xtime_lock);
}

#if 0
static struct irqaction timer_irq = {
	.name	= "timer",
	.flags	= SA_INTERRUPT,
};
#endif

/*
 * Include architecture specific code
 */
#include <asm/arch/time.h>

/*
 * This must cause the timer to start ticking.
 * It doesn't have to set the current time though
 * from an RTC - it can be done later once we have
 * some buses initialised.
 */
void __init time_init(void)
{
	rtc_init_startup();
	xtime.tv_usec = 0;
	xtime.tv_sec  = get_cmos_time();

	setup_timer();
}

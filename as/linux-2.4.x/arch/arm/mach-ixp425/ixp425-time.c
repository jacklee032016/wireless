/*
 * arch/arm/mach-ixp425/ixp425-time.c
 *
 * Timer tick for IXP425 based sytems. We use OS timer1 on the CPU for
 * the timer tick and the timestamp counter to account for missed jiffies.
 *
 * Author:  Peter Barry
 * Copyright:   (C) 2001 Intel Corporation.
 * 		(C) 2002-2003 MontaVista Software, Inc.
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 $Author: lizhijie $
$Log: ixp425-time.c,v $
Revision 1.1.1.1  2006/11/29 08:55:09  lizhijie
AS600 Kernel

Revision 1.1.1.1  2005/07/08 09:46:05  lizhijie
Linux-2.4.24 integrated Intel Access Library 2.0

Revision 1.6  2005/06/07 10:27:37  lizhijie
remove watchdog suuport from kernel

Revision 1.5  2005/05/30 03:31:09  lizhijie
no message

Revision 1.4  2005/04/19 03:43:00  wangwei
通过更改ixp425_time.c中的localtime()函数解决日期错乱问题

Revision 1.3  2005/04/18 07:48:45  wangwei
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

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif
extern int setup_arm_irq(int, struct irqaction *);
static unsigned volatile last_jiffy_time;
unsigned long last_run_time;
#define	USEC_PER_SEC		1000000
#define CLOCK_TICKS_PER_USEC	(CLOCK_TICK_RATE / USEC_PER_SEC)

struct tm
{
	int tm_sec;                   // Seconds.     [0-60] (1 leap second) 
	int tm_min;                   // Minutes.     [0-59] 
	int tm_hour;                  // Hours.       [0-23] 
	int tm_mday;                  // Day.         [1-31]
	int tm_mon;                   // Month.       [0-11] 
	int tm_year;                  // Year - 1900.  
	int tm_wday;                  // Day of week. [0-6] 
	int tm_yday;                  // Days in year.[0-365] 
	int tm_isdst;                 // DST.         [-1/0/1]

	long int tm_gmtoff;           // we don't care, we count from GMT 
	const char *tm_zone;          // we don't care, we count from GMT 
};

#define localtime_2
#ifdef localtime_1
#define SPD 24*60*60

static void
localtime(long *timepr, struct tm *r) {
	long i;
	long timep;
	long work;
	long k;
	struct timezone sys_tz;
	unsigned int __spm[12] =
		{ 0,
		  (31),
		  (31+28),
		  (31+28+31),
		  (31+28+31+30),
		  (31+28+31+30+31),
		  (31+28+31+30+31+30),
		  (31+28+31+30+31+30+31),
		  (31+28+31+30+31+30+31+31),
		  (31+28+31+30+31+30+31+31+30),
		  (31+28+31+30+31+30+31+31+30+31),
		  (31+28+31+30+31+30+31+31+30+31+30),
		};
	
	timep = (*timepr) - (sys_tz.tz_minuteswest * 60);
	work=timep%(SPD);
	r->tm_sec=work%60; work/=60;
	r->tm_min=work%60;
	r->tm_hour=work/60;
	work=timep/(SPD);
	r->tm_wday=(4+work)%7;
	for (i=1970; ; ++i) 
	{
		k= (!(i%4) && ((i%100) || !(i%400)))?366:365;
		if (work>k)
			work-=k;
		else
			break;
	}
	r->tm_year=i-1900;
	for (i=11; i && __spm[i]>work; --i) ;
	r->tm_mon=i;
	r->tm_mday=work-__spm[i]+1;
}
#endif


#ifdef localtime_2
#define __int32 long
const char Days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static void
ww_dcalllocaltimes(long time,long timezone,struct tm *tm_time)
{    
	unsigned  __int32 n32_Pass4year;      
	__int32 n32_hpery;    
	//计算时差  
	time=time-timezone;        
	if(time < 0)     
	{       
		time = 0;    
	}  
	//取秒时间
	tm_time->tm_sec=(int)(time % 60);    
	time /= 60;  
	//取分钟时间
	tm_time->tm_min=(int)(time % 60);    
	time /= 60; 
	//取过去多少个四年，每四年有 1461*24 小时  
	n32_Pass4year=((unsigned int)time / (1461L * 24L));   
	//计算年份   
	tm_time->tm_year=(n32_Pass4year << 2)+70;   
	//四年中剩下的小时数  
	time %= 1461L * 24L;   
	//校正闰年影响的年份，计算一年中剩下的小时数
	for (;;)    
	{       
		//一年的小时数       
		n32_hpery = 365 * 24;      
		//判断闰年       
		if ((tm_time->tm_year & 3) == 0)		
		{           
			//是闰年，一年则多24小时，即一天         
			n32_hpery += 24;		
		}      
		if (time < n32_hpery)		
		{            
			break;		
		}        

		tm_time->tm_year++;      
		time -= n32_hpery;   
	}   
	//小时数   
	tm_time->tm_hour=(int)(time % 24);    
	//一年中剩下的天数  
	time /= 24;    
	//假定为闰年    
	time++;  
	//校正润年的误差，计算月份，日期   
	if ((tm_time->tm_year & 3) == 0)    
	{		
		if (time > 60)		
		{			
			time--;		
		}		
		else		
		{			
			if (time == 60)			
			{				
				tm_time->tm_mon = 1;				
				tm_time->tm_mday = 29;			
				return ;			
			}		
		}  
	}   
	//计算月日  
	for (tm_time->tm_mon = 0; Days[tm_time->tm_mon] < time;tm_time->tm_mon++)   
	{         
		time -= Days[tm_time->tm_mon];   
	}   
	tm_time->tm_mday = (int)(time);    
	return;
}
#endif


/* IRQs are disabled before entering here from do_gettimeofday() */
static unsigned long ixp425_gettimeoffset(void)
{
	u32 elapsed, usec, curr, reload;
	volatile u32 stat1, stat2;

	elapsed = *IXP425_OSTS - last_jiffy_time;

	return elapsed / CLOCK_TICKS_PER_USEC;
}

/* dallas RTC chip function decaration, Li Zhijie, 2005.05.27*/
void dallas_set_register(unsigned char address, unsigned char value);
/* end of added by lizhijie */

/*
 * In order to set the CMOS clock precisely, set_rtc_mmss has to be
 * called 500 ms after the second nowtime has started, because when
 * nowtime is written into the registers of the CMOS clock, it will
 * jump to the next second precisely 500 ms later. Check the Motorola
 * MC146818A or Dallas DS12887 data sheet for details.
 *
 * BUG: This routine does not handle hour overflow properly; it just
 *      sets the minutes. Usually you'll only notice that after reboot!
 */
static void set_cmos_time(void)
{
	struct tm dr;
	unsigned char temp;
	#ifdef localtime_1
	localtime(&xtime.tv_sec,&dr);
	#endif 
	#ifdef localtime_2
	ww_dcalllocaltimes(xtime.tv_sec,0,&dr);
	#endif
	BIN_TO_BCD(dr.tm_sec);
	BIN_TO_BCD(dr.tm_min);
	BIN_TO_BCD(dr.tm_hour);
	BIN_TO_BCD(dr.tm_mday);
	BIN_TO_BCD(dr.tm_mon);
	BIN_TO_BCD(dr.tm_year);
	temp=dr.tm_sec;
	dallas_set_register(0x80,temp);
	temp=dr.tm_min;
	dallas_set_register(0x82,temp);
	temp=dr.tm_hour;
	dallas_set_register(0x84,temp);
	temp=dr.tm_mday;
	dallas_set_register(0x86,temp);
	temp=dr.tm_mon+1;
	if(temp==0x0a)  temp=0x10;
	dallas_set_register(0x88,temp);
	temp=dr.tm_year;
	dallas_set_register(0x8c,temp);

	#if 0
	printk("change time succed---(mm/dd/yy    hh:mm:ss)        %x/%x/%x    %x:%x:%x\n",dallas_get_register(0x89),dallas_get_register(0x87),dallas_get_register(0x8d),dallas_get_register(0x85),dallas_get_register(0x83),dallas_get_register(0x81));
	#endif
	return ;
}
static void ixp425_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags=0;

	/* Clear Pending Interrupt by writing '1' to it */
	*IXP425_OSST = IXP425_OSST_TIMER_1_PEND;

 	if(last_run_time>xtime.tv_sec)
	{
		flags=last_run_time-xtime.tv_sec;
	}
	if(xtime.tv_sec>last_run_time)
	{
	 	flags=xtime.tv_sec-last_run_time;
	}
	if((last_run_time!=0)&&(flags>1))
	{
		set_cmos_time();
	}
	last_run_time=xtime.tv_sec;
	/*
	 * Catch up with the real idea of time
	 */
	while((*IXP425_OSTS - last_jiffy_time) > LATCH) 
	{
		do_timer(regs);
		last_jiffy_time += LATCH;
	}		

/*  remove watchdog support , lizhijie, 2005.06.07 */
#if 0
	/* reset the count value of WatchDog count register, li zhijie,2005.05.28 */
	*IXP425_OSWT = 0X5000000;	/* about 1.26 second  */
	/* end of added by Li Zhijie */
#endif

}

extern unsigned long (*gettimeoffset)(void);

static struct irqaction timer_irq = {
	name: "Timer Tick",
	flags: SA_INTERRUPT
};
/* function declaration for ixp watchdog, lizhijie, 2005.05.28 */
void __init as_ixp_watchdog_setup(void);
/* end of added by lizhijie */

void __init setup_timer(void)
{
	gettimeoffset = ixp425_gettimeoffset;
	timer_irq.handler = ixp425_timer_interrupt;

	/* Clear Pending Interrupt by writing '1' to it */
	*IXP425_OSST = IXP425_OSST_TIMER_1_PEND;

	/* Setup the Timer counter value */
	*IXP425_OSRT1 = (LATCH & ~IXP425_OST_RELOAD_MASK) | IXP425_OST_ENABLE;

	/* Reset time-stamp counter */
	*IXP425_OSTS = 0;
	last_jiffy_time = 0;

	/* Connect the interrupt handler and enable the interrupt */
	setup_arm_irq(IRQ_IXP425_TIMER1, &timer_irq);

	/* remove WatchDog support, lizhijie, 2005.06.07 */
#if 0	
	/* added by Li Zhijie, 2005.05.27 */
	as_ixp_watchdog_setup();
	/* end of added */
#endif	
}



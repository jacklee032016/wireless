/* 
* This file is added by Li Zhijie, 2005.03.31 for the purpose of real-time clock
* RTC chip used on our board is DALLAS DS1302z Trickle Charge Timekeeping Chip
$Author: lizhijie $
$Log: dallas_rtc.c,v $
Revision 1.1.1.1  2006/11/29 08:55:09  lizhijie
AS600 Kernel

Revision 1.1.1.1  2005/07/08 09:46:05  lizhijie
Linux-2.4.24 integrated Intel Access Library 2.0

Revision 1.3  2005/04/19 08:03:09  wangwei
修改了在编译过程中警告的地方

Revision 1.2  2005/04/18 07:48:37  wangwei
no message

$Revision: 1.1.1.1 $

*/


#include <linux/module.h>

#include <asm/hardware.h>

#include <linux/sched.h>


#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)


#define  DALLAS_RTC_DATA 		IXP425_GPIO_PIN_1
#define  DALLAS_RTC_SCLK		IXP425_GPIO_PIN_2
#define  DALLAS_RTC_CS			IXP425_GPIO_PIN_3

#if 0 
#define DALLAS_COMMAND_TAG			( 1<<7 )
#define DALLAS_COMMAND_CALENDER		( 0<<6 )
#define DALLAS_COMMAND_RAM			( 1<<6 )

#define DALLAS_COMMAND_WRITE			( 0 )
#define DALLAS_COMMAND_READ			( 1 )
#endif

#define READ_YEAR_REGISTER 		0x8d
#define READ_MONTH_REGISTER 		0x89
#define READ_DAY_REGISTER			0x87
#define READ_HR_REGISTER			0x85
#define READ_MIN_REGISTER			0x83
#define READ_SEC_REGISTER			0x81

#define WRITE_YEAR_REGISTER		0x8c
#define WRITE_MONTH_REGISTER		0x88
#define WRITE_DAY_REGISTER			0x86
#define WRITE_HR_REGISTER			0x84
#define WRITE_MIN_REGISTER			0x82
#define WRITE_SEC_REGISTER			0x80

#define TRICKLE_CHARGER_REGISTER 				0x90
#define TRICKLE_CHARGER_REGISTER_VALUE 		0xaa

int dallas_get_register(unsigned char address);
void dallas_set_register(unsigned char address, unsigned char value);
void inline nowtime_goto_xtime();
unsigned long get_cmos_time(void);
void rtc_init_startup();
void inline __rtc_chip_select();
void inline __rtc_chip_deselect();



void rtc_init_startup()
{
	char  get_value;
	dallas_set_register(TRICKLE_CHARGER_REGISTER,TRICKLE_CHARGER_REGISTER_VALUE);

	get_value=dallas_get_register(READ_SEC_REGISTER);
	get_value&=0x7f;
	dallas_set_register(WRITE_SEC_REGISTER,get_value);
}


/* select RTC chip , eg. CS signal is effective */
void inline __rtc_chip_select()
{
	gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_LOW );
	gpio_line_set(DALLAS_RTC_CS, IXP425_GPIO_HIGH);
}
/* deselect RTC chip */
void inline __rtc_chip_deselect()
{
	gpio_line_set(DALLAS_RTC_CS, IXP425_GPIO_LOW);
}

void __rtc_init()
{
	gpio_line_config(DALLAS_RTC_CS,IXP425_GPIO_OUT);
	gpio_line_config(DALLAS_RTC_SCLK,IXP425_GPIO_OUT);
	gpio_line_config( DALLAS_RTC_DATA,IXP425_GPIO_OUT );
	__rtc_chip_deselect();
	gpio_line_set(DALLAS_RTC_SCLK,IXP425_GPIO_LOW); 
}


void inline nowtime_goto_xtime()
{
	xtime.tv_sec =get_cmos_time() ;
}


/* not static: needed by APM */
unsigned long get_cmos_time(void)
{
	unsigned int year, mon, day, hour, min, sec;

	sec = dallas_get_register(READ_SEC_REGISTER);
	min = dallas_get_register(READ_MIN_REGISTER);
	hour = dallas_get_register(READ_HR_REGISTER);
	day = dallas_get_register(READ_DAY_REGISTER);
	mon = dallas_get_register(READ_MONTH_REGISTER);
	year = dallas_get_register(READ_YEAR_REGISTER);

	BCD_TO_BIN(sec);
	BCD_TO_BIN(min);
	BCD_TO_BIN(hour);
	BCD_TO_BIN(day);
	BCD_TO_BIN(mon);
	BCD_TO_BIN(year);

	if ((year += 1900) < 1970)  year += 100;

	return mktime(year, mon, day, hour, min, sec);
}


/*
 *   value if set 0 , colse write protect ; 
 *   value if set 1,  open write protect ;
 */
static inline void _rtc_write_protect(unsigned char value)
{
	char i;
	char control=0x8e;
	char x=0;
	int *get_value;
	get_value=&value;
	
	__rtc_init();
	
	__rtc_chip_select();

	for (i=0;i<8;i++) 
	{
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_LOW );
		x=control&0x01;
		if(x)   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_HIGH );
		else   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_LOW );
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_HIGH );
		control=control>>1;
	}
	
	control=value;
	for (i=0;i<8;i++) 
	{
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_LOW );
		x=control&0x01;
		if(x)   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_HIGH );
		else   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_LOW );
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_HIGH );
		control=control>>1;
	}
	
	gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_LOW );
	__rtc_chip_deselect();
}




int dallas_get_register(unsigned char address)
{
	char  x=0;
	int i=0;
	int tmp_v;
	int *tmp_value;
	tmp_value=&tmp_v;
	
	__rtc_init();
	__rtc_chip_select();
	
	for (i=0;i<8;i++) 
	{
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_LOW );
		x=address&0x01;
		if(x)   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_HIGH );
		else   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_LOW );
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_HIGH );
		address=address>>1;
	}
	
	gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_LOW );
	gpio_line_config( DALLAS_RTC_DATA, IXP425_GPIO_IN );
	x=0;
	for (i=0;i<8;i++) 
	{
		x=x>>1;
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_HIGH );
		gpio_line_get(DALLAS_RTC_DATA, tmp_value);
		if(tmp_v!=0)    
		{
			x=x+0x80;
		}
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_LOW );
	}
	
	gpio_line_set(DALLAS_RTC_SCLK,  IXP425_GPIO_LOW);
	__rtc_chip_deselect();
	tmp_v=0;
	tmp_v=x;
	return x;
}



void dallas_set_register(unsigned char address, unsigned char value)
{
	unsigned char x=0;
	unsigned char i=0;
	int value1;
	int *get_value;
	get_value=&value1;
	
	_rtc_write_protect(0);
	
	__rtc_init();
	
	__rtc_chip_select();

	for (i=0;i<8;i++) 
	{
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_LOW );
		x=address&0x01;
		if(x)   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_HIGH );
		else   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_LOW );
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_HIGH );
		address=address>>1;
	}
	
	address=value;
	for (i=0;i<8;i++) 
	{
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_LOW );
		x=address&0x01;
		if(x)   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_HIGH );
		else   gpio_line_set( DALLAS_RTC_DATA, IXP425_GPIO_LOW );
		gpio_line_set( DALLAS_RTC_SCLK, IXP425_GPIO_HIGH );
		address=address>>1;
	}

	__rtc_chip_deselect();

	_rtc_write_protect(1);
}

#if 0
/*write and read RAM of DS1302*/
void write_1302ram(unsigned char add,unsigned char b) 
{
     unsigned char a;
     a=add<<1;
     add=a|0xc0;  
     dallas_set_register(add,b);
}

 unsigned char read_1302ram(unsigned char add) 
{
     unsigned char a,b,c;
	unsigned char *pp;
	pp=&c;
     a=add;
     a=a<<1;
     b=a|0xc1;
     dallas_get_register(b,*pp);
     return(*pp);
}
#endif



int as_dallas_rtc_init(void)
{
	rtc_init_startup();
	return 0;
}

void as_dallas_rtc_exit(void)
{
	#if 0
  	printk("test DS1302 is over !");
	#endif
	return ;
}


void module_init(as_dallas_rtc_init);

void module_exit(as_dallas_rtc_exit);





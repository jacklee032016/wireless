
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

#define WAIT_TIME_SE	1

#define PIN_CS                           	IXP425_GPIO_PIN_0
#define PIN_SCK         				IXP425_GPIO_PIN_4
#define PIN_DIN         	 			IXP425_GPIO_PIN_2
#define PIN_DOUT                           	IXP425_GPIO_PIN_1
/** D E F I N E S **************************************************/
#define EWEN_CMD    		0x98//0b10011000          // EWEN command
#define EWDS_CMD    		0x80//0b10000000          // EWDS command
#define WRITE_CMD   		0xa0//0b10100000          // WRITE command
#define READ_CMD    		0xc0//0b11000000          // READ command
/* This constant is used to determine how many bits are transferred 
* during each command. The provided value is for a 93XX66A device, 
* and must be altered if a different device is to be used. */
#define NUMBITS     11                  // # of bits of EWEN cmd.
/** V A R I A B L E S **********************************************/
static unsigned char command;                  // Command byte variable
static unsigned int address;                   	// Address word variable
static unsigned char buffer;                   	// I/O buffer variable

/** P R O T O T Y P E S ********************************************/
static void waitw(int xx);
static void Ewen(void);
//static void WriteX8(unsigned char data);
static void WriteX16(unsigned int data);
//static unsigned char ReadX8(void);
static unsigned int ReadX16(void);
static void Ewds(void);
static int as_get_gpio_value(void );
static void poll(void);
static void bitout(void);
static void bitin(void);
static void byteout(void);
static void bytein(void);
static void SendCommand(void);
static int write_93lc66(void);
static int read_93lc66(void);
/********************************************************/
static void waitw(int xx)
{
	int yy;
	for(yy=0;yy<xx;yy++)
	{
		yy++;yy--;
	}
}
/********************************************************************
 * Function:        void Ewen(void)
 *
 * Description:     This function sends an EWEN command to the device.
 *                  Once this command has been given, writing to the
 *                  device array will be enabled, and will remain as
 *                  such until an EWDS command is given or power is
 *                  removed from the device.
 *******************************************************************/
static void Ewen(void)
{
    	command = EWEN_CMD;                 		
    	address = 0;                        			
		
    	/* Enable Chip Select*/
	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_HIGH);
	
    	SendCommand();                      		
		
    	/* Disable Chip Select*/
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_LOW);
}
#if 0
/********************************************************************
 * Function:        void WriteX8(unsigned char data)
 *
 * Description:     This function writes the 8-bit value stored in
 *                  data to the serial EEPROM device, at the location
 *                  specified by address.
 *******************************************************************/
static void WriteX8(unsigned char data)
{
    	command = WRITE_CMD;                		// Load WRITE command value
    	// Enable Chip Select
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_HIGH);
    	SendCommand();                      		// Output command to device
    	buffer = data;                      			// Copy data to buffer
    	byteout();                          			// Output byte
    	// Disable Chip Select
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_LOW);

    	poll();                             				// Begin ready/busy polling
}

/********************************************************************
 * Function:        unsigned char ReadX8(void)
 *
 * Description:     This function reads an 8-bit value from the
 *                  serial EEPROM device, from the location
 *                  specified by address, returns it.
 *******************************************************************/
static unsigned char ReadX8(void)
{
    	command = READ_CMD;                 		// Load READ command value
    	//CS = 1;                             			// Enable Chip Select
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_HIGH);
    	SendCommand();                      		// Output command to device
    	bytein();                           				// Input byte from device
    	//CS = 0;                             			// Disable Chip Select
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_LOW);
    	return buffer;                      				// Return value
}
#endif
/********************************************************************
 * Function:        void WriteX16(unsigned int data)
 *
 * Description:     This function writes the 16-bit value stored in
 *                  data to the serial EEPROM device, at the location
 *                  specified by address.
 *******************************************************************/
static void WriteX16(unsigned int data)
{
    	command = WRITE_CMD;                	
		
    	/* Enable Chip Select*/
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_HIGH);
	
    	SendCommand();                      		
		
    	buffer = (char)(data >> 8);         		
    	byteout();                          			
    	buffer = (char)data;                			
    	byteout();                          		
		
    	/* Disable Chip Select*/
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_LOW);

    	poll();                             				
} 
/********************************************************************
 * Function:        unsigned int ReadX16(void)
 *
 * Description:     This function reads a 16-bit value from the
 *                  serial EEPROM device, from the location
 *                  specified by address, returns it.
 *******************************************************************/
static unsigned int ReadX16(void)
{
    	unsigned int retval;                

    	command = READ_CMD;                 

    	/* Enable Chip Select*/
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_HIGH);
	
    	SendCommand();                      
		
    	bytein();                           
    	retval = buffer;                   
    	bytein();                           
    	retval = (retval << 8) | buffer;    
		
    	/* Disable Chip Select*/
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_LOW);

    	return retval;                      
}

/********************************************************************
 * Function:        void Ewds(void)
 *
 * Description:     This function sends an EWDS command to the device.
 *                  It is good practice to always send this command
 *                  after completing a write, so as to avoid the
 *                  corruption of data stored in the device.
 *******************************************************************/
static void Ewds(void)
{
    	command = EWDS_CMD;                 
    	address = 0;             
		
    	/* Enable Chip Select*/
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_HIGH);
	
    	SendCommand();                      
		
    	/* Disable Chip Select*/
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_LOW);
}


static int as_get_gpio_value(void )
{	
	int tmp_v;	
	int *tmp_value;	
	
	tmp_value=&tmp_v;	
	gpio_line_config(PIN_DIN,IXP425_GPIO_IN);	
	gpio_line_get(PIN_DIN, tmp_value);	
	
	return tmp_v;
}
/********************************************************************
 * Function:        void poll(void)
 *
 * Description:     This function brings CS high to initiate the
 *                  Ready/Busy polling feature. DO is then continuously
 *                  polled to see when it goes high, thus indicating
 *                  that the write cycle has completed.
 *******************************************************************/
static void poll(void)
{
    	/* Set CS high*/
     	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_HIGH);
	
	waitw(WAIT_TIME_SE);
	
	while (as_get_gpio_value() == 0) {};      	
	
    	/* Bring CS low*/
    	gpio_line_config(PIN_CS,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_CS,IXP425_GPIO_LOW);
}

/********************************************************************
 * Function:        void bitout(void)
 *
 * Description:     This function outputs the MSb of buffer to the
 *                  serial EEPROM device.
 *******************************************************************/
static void bitout(void)
{
    	if (buffer & 0x80)                  				
    	{
        	/* If so, send a 1*/
        	gpio_line_config(PIN_DOUT,IXP425_GPIO_OUT);		
		gpio_line_set(PIN_DOUT,IXP425_GPIO_HIGH);
    	}
    	else                                					
    	{
        	/* Send a 0*/
         	gpio_line_config(PIN_DOUT,IXP425_GPIO_OUT);		
		gpio_line_set(PIN_DOUT,IXP425_GPIO_LOW);
    	}
		
    	/* Bring SCK high to latch data*/
     	gpio_line_config(PIN_SCK,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_SCK,IXP425_GPIO_HIGH);
	
	waitw(WAIT_TIME_SE);
	
	/* Bring SCK low for next bit*/
    	gpio_line_config(PIN_SCK,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_SCK,IXP425_GPIO_LOW);
} 

/********************************************************************
 * Function:        void bitin(void)
 *
 * Description:     This function inputs a bit from the serial EEPROM
 *                  device and stores it in the LSb of buffer.
 *******************************************************************/
static void bitin(void)
{
    	buffer &= 0xFE;                     			
		
    	/* Bring SCK to latch data*/
    	gpio_line_config(PIN_SCK,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_SCK,IXP425_GPIO_HIGH);
	
	waitw(WAIT_TIME_SE);
	
	/* Bring SCK low*/
      	gpio_line_config(PIN_SCK,IXP425_GPIO_OUT);		
	gpio_line_set(PIN_SCK,IXP425_GPIO_LOW);
    	if (as_get_gpio_value() == 1)         	
    	{
        	buffer |= 0x01;                 			
    	}
}

/********************************************************************
 * Function:        void byteout(void)
 *
 * Description:     This function outputs the byte specified in
 *                  buffer to the serial EEPROM device.
 *******************************************************************/
static void byteout(void)
{
    	unsigned char i;                    			

    	for (i = 0; i < 8; i++)             			
    	{
        	bitout();                       			
        	buffer = buffer << 1;           		
    	}
}

/********************************************************************
 * Function:        void bytein(void)
 *
 * Description:     This function inputs a byte from the serial
 *                  EEPROM device and stores it in buffer.
 *******************************************************************/
static void bytein(void)
{
    	unsigned char i;                    

    	buffer = 0;
    	for (i = 0; i < 8; i++)             
    	{
        	buffer = buffer << 1;           
        	bitin();                        
    	}
}

/********************************************************************
 * Function:        void SendCommand(void)
 * 
 * Description:     This function sends the Start bit and opcode
 *                  specified in the MSb's of command, as well as
 *                  the required number of address or dummy bits,
 *                  to the serial EEPROM device.
 *******************************************************************/
static void SendCommand(void)
{
    	static unsigned char i;             
    	static unsigned int cmd_addr;       
    	static unsigned int temp;           

    	cmd_addr = address;                 
    	/* First, align address bits to be combined with command*/
    	for (i = 0; i < (16-NUMBITS); i++)  
    	{
        	cmd_addr = cmd_addr << 1;       
    	}

    	/* Next, combine command into address word*/
    	cmd_addr &= 0x1FFF;                
    	temp = command;                     
    	cmd_addr |= (temp<<8);              

    	/* Finally, output entire command to device*/
    	for (i = 0; i < NUMBITS; i++)       
    	{
        	buffer = (char)(cmd_addr >> 8); 
        	bitout();                       
        	cmd_addr = cmd_addr << 1;      
    	}
} 

static int write_93lc66(void)
{
	unsigned int data;
	int x;

	Ewen();

	address = 0x00; data=0x4154; WriteX16(data);
	
	address = 0x01; data=0x840f; WriteX16(data);
	address = 0x02; data=0x40f; WriteX16(data);
	address = 0x03; data=0x840f; WriteX16(data);
	address = 0x04; data=0x40f; WriteX16(data);
	address = 0x05; data=0x840f; WriteX16(data);
	address = 0x06; data=0x40f; WriteX16(data);
	address = 0x07; data=0x840f; WriteX16(data);
	address = 0x08; data=0x880f; WriteX16(data);
	address = 0x09; data=0x881f; WriteX16(data);
	
	address = 0x0a; data=0x5902; WriteX16(data);
	address = 0x0b; data=0x8000; WriteX16(data);
	address = 0x0c; data=0xfa50; WriteX16(data);
	address = 0x0d; data=0xfa50; WriteX16(data);
	address = 0x0e; data=0x5500; WriteX16(data);
	address = 0x0f; data=0x5500; WriteX16(data);
	
	address = 0x10; data=0x0040; WriteX16(data);
	address = 0x11; data=0xff30; WriteX16(data);
	address = 0x12; data=0x3600; WriteX16(data);
	
	address = 0x13; data=0xffff; WriteX16(data);
	address = 0x14; data=0x0155; WriteX16(data);
	address = 0x15; data=0x0180; WriteX16(data);
	address = 0x16; data=0xffff; WriteX16(data);
	address = 0x17; data=0xffff; WriteX16(data);
	address = 0x18; data=0xffff; WriteX16(data);
	address = 0x19; data=0xffff; WriteX16(data);
	address = 0x1a; data=0xffff; WriteX16(data);
	address = 0x1b; data=0xffff; WriteX16(data);
	address = 0x1c; data=0xffff; WriteX16(data);
	address = 0x1d; data=0xffff; WriteX16(data);
	address = 0x1e; data=0xffff; WriteX16(data);
	address = 0x1f; data=0xffff; WriteX16(data);
	address = 0x20; data=0xffff; WriteX16(data);
	address = 0x21; data=0xffff; WriteX16(data);
	address = 0x22; data=0xffff; WriteX16(data);
	
	address = 0x23; data=0x0000; WriteX16(data);
	address = 0x24; data=0x0000; WriteX16(data);
	address = 0x25; data=0x0000; WriteX16(data);
	address = 0x26; data=0x0000; WriteX16(data);
	address = 0x27; data=0x0000; WriteX16(data);
	
	address = 0x28; data=0x0000; WriteX16(data);
	address = 0x29; data=0x0000; WriteX16(data);
	address = 0x2a; data=0x0000; WriteX16(data);
	address = 0x2b; data=0x0000; WriteX16(data);
	address = 0x2c; data=0xd000; WriteX16(data);
	
	address = 0x2d; data=0x4442; WriteX16(data);
	address = 0x2e; data=0x0000; WriteX16(data);
	address = 0x2f; data=0x0000; WriteX16(data);
	
	address = 0x30; data=0x09e7; WriteX16(data);
	address = 0x31; data=0x0000; WriteX16(data);
	address = 0x32; data=0x0000; WriteX16(data);
	address = 0x33; data=0x0000; WriteX16(data);


	for(address=0x34;address<0x100;address++)
	{
		WriteX16(0xffff);
	}
	
	Ewds();  

	address = 0;
	for(x=0;x<0x100;x++)
	{
		data = ReadX16(); 
		printk("93lc66' address 0x%x is 0x%x\n",address,data);
		address++;
	}

	return 0;
}

static int read_93lc66(void)
{
	unsigned int data;
	int x;
	address = 0;
	for(x=0;x<0x100;x++)
	{
		data = ReadX16(); 
		printk("93lc66' address 0x%x is 0x%x\n",address,data);
		address++;
	}
	return 0;
}

int as_93lc66_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	as_misc_dev	*dev;

	if (off > 0)
		return 0;
		
	dev = (as_misc_dev *)data;
	if (!dev)
		return 0;

	MOD_INC_USE_COUNT;

	len += sprintf(page + len,"Assist 93LC66\n");

	MOD_DEC_USE_COUNT;
	
	return len;
}

int as_93lc66_proc_write(struct file *file, const char *buf, unsigned long count, void *data)
{
	AS_93LC66_COMMAND cmd;
	int len;

	MOD_INC_USE_COUNT;

	len = copy_from_user(&cmd, buf, sizeof(AS_93LC66_COMMAND ));
	if( len )
	{
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	switch(cmd.type)
	{
		case AS_93LC66_WRITE:
			write_93lc66();
			break;
			
		case AS_93LC66_READ:
			read_93lc66();
			break;
			
		default:
			printk(KERN_INFO "Not validate command for Assist 93LC66 Driver\r\n");
			len = 0;
			break;
	}
	MOD_DEC_USE_COUNT;

	return len;
}

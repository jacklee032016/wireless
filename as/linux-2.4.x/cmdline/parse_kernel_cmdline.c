#define CMD_LINE_LENGTH			256
#define MAC_ADDRESS_LENGTH		17
#define IEEE803_MAC_ADDRESS_SIZE	6

#define ASSIST_ETH_DEBUG			1

typedef struct  
{
    unsigned char macAddress[IEEE803_MAC_ADDRESS_SIZE]; /**< MAC address */
} Assist_EthMacAddr;

#ifdef __KERNEL__
#include <linux/module.h>
#define PRINT	printk
int assist_eth_configed = 0;

#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define PRINT	printf
#endif

static Assist_EthMacAddr mac_addr_1, mac_addr_2;

#if ASSIST_ETH_DEBUG
static void assist_debug_eth_mac(Assist_EthMacAddr *addr)
{
	PRINT("Mac is %2x:%2x:%2x:%2x:%2x:%2x\r\n", 
		addr->macAddress[0], addr->macAddress[1], addr->macAddress[2],
		 addr->macAddress[3], addr->macAddress[4], addr->macAddress[5]);
}
#endif

static unsigned char assist_aton(unsigned char ch)
{
	unsigned char value = 255;
	
	if(ch>47 && ch<58)
		value = ch - 48;
	if(ch>64/*'@'*/ && ch<71/*'G'*/)
		value = ch-55;
	if(ch>96 && ch<103)
		value = ch-87;
	return value;
}

static int assist_get_eth_mac_addr(Assist_EthMacAddr *addr, char *mac)
{
	unsigned char *value = addr->macAddress;
	unsigned char *line =mac;
	unsigned char ch1,ch2;
	int i=0;
	
	for(i=0; i<IEEE803_MAC_ADDRESS_SIZE; i++)
	{
		ch1 = assist_aton(line[0]);
		ch2 = assist_aton(line[1]);
		if(ch1==255 || ch2==255)
			return -1;
		value[i] = (ch1<< 4) | ch2;
		line += 3;
	}
#if ASSIST_ETH_DEBUG
	assist_debug_eth_mac( addr);
#endif	
	return 0;
}

int assist_parse_cmd_line(unsigned char *cmdline, unsigned int index)
{
	unsigned char *s;
	unsigned char mac_index[20];
	unsigned char mac[MAC_ADDRESS_LENGTH+1];
	Assist_EthMacAddr *addr;

	if(index == 1)
		addr = &mac_addr_1;
	else if(index==2)
		addr = &mac_addr_2;
	else
	{
		PRINT("MAC index %d is out of range\r\n", index);
		return -1;
	}

#if ASSIST_ETH_DEBUG
	sprintf(mac_index, "mac%d", index);
#endif
	memset(mac, 0, MAC_ADDRESS_LENGTH+1);
	s = strstr(cmdline, mac_index);
	if(!s)
	{
		PRINT("No %s is found\r\n", mac_index);
		return -1;
	}

	strncpy(mac, s+5, MAC_ADDRESS_LENGTH);
	mac[MAC_ADDRESS_LENGTH]= '\0';
#if ASSIST_ETH_DEBUG
	PRINT("%s is '%s'\r\n", mac_index, mac);
#endif
	return assist_get_eth_mac_addr(addr, mac);
}

int assist_parse_kernel_cmdline(unsigned char *cmdline)
{
	if(assist_parse_cmd_line( cmdline, 1 ) )
		return -1;
	if(assist_parse_cmd_line( cmdline, 2 ) )
		return -1;
#ifdef __KERNEL__
	assist_eth_configed = 1;
#endif

	return 0;
}

#ifdef __KERNEL__

#if MAC_BOOTLOADER 

int __init assist_init_hardware_address(void)
{
	extern char saved_command_line[];
	return assist_parse_kernel_cmdline(saved_command_line);
}

int assist_get_hardware_address(int dev_index, unsigned char *mac_value)
{
	Assist_EthMacAddr *addr;
	if(!assist_eth_configed)
	{
#if ASSIST_ETH_DEBUG
		PRINT("dev index is '%d', but hardware address is not inited now\r\n", dev_index);
#endif
		return -1;
	}
	
#if ASSIST_ETH_DEBUG
	PRINT("dev index is '%d'\r\n", dev_index);
#endif
	if(dev_index==0)
		addr = &mac_addr_1;
	else
		addr = &mac_addr_2;
		
	memcpy(mac_value, addr->macAddress, IEEE803_MAC_ADDRESS_SIZE);
	
	return 0;
}


EXPORT_SYMBOL(assist_get_hardware_address);
#endif

#else
int main(int argc, char *argv[])
{
	int fd;
	unsigned char cmdline[CMD_LINE_LENGTH];
	int len;
	
	fd = open("/proc/cmdline", O_RDONLY);
	if(fd<0)
	{
		printf("/proc/cmdline open failed\r\n");
		exit(1);
	}

	memset(cmdline, 0, CMD_LINE_LENGTH);
	len = read(fd, cmdline, CMD_LINE_LENGTH);
	if(len<0)
	{
		printf("/proc/cmdline read failed\r\n");
		exit(1);
	}
	close(fd);

#if ASSIST_ETH_DEBUG
	printf("kernel cmdline is '%s', length is %d\r\n", cmdline, len);
#endif

	assist_parse_kernel_cmdline( cmdline);
	return 0;
}

#endif


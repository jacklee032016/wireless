#include <stdio.h>

int main(int argc, char *argv[])
{
	short x = 512;//0xabcd;
	int y= 288240;//0017;//0xabcdef12;

	char a, b , c, d;
	
	printf("Length of char is  : %d\r\n" , sizeof(unsigned char));
	printf("Length of short is  : %d\r\n" , sizeof(short));
	printf("Length of Integer is  : %d\r\n" , sizeof(int));

	printf("Short is  : %x\r\n" , x);
	a = x&0xFF;
	b = (x>>8)&0xFF;
	printf("High byte is  : %x\r\n" , b);
	printf("Low byte is  : %x\r\n" , a);
	
	printf("Int is  : %x\r\n" , y);
	a = y&0xFF;
	b = (y>>8)&0xFF;
	c = (y>>16)&0xFF;
	d = (y>>24)&0xFF;
	printf("Highest byte is  : %x\r\n" , d);
	printf("Higher byte is  : %x\r\n" , c);
	printf("Lowwer byte is  : %x\r\n" , b);
	printf("Lowest byte is  : %x\r\n" , a);
	return 0;
}


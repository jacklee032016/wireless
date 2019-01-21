/*
 * drivers/mtd/maps
 *
 * MTD Map file for IXP425 based systems
 *
 * Original Author: Intel Corporation
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright 2002 Intel Corporation
 *
 */


#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/ioport.h>
#include <asm/io.h>

#define WINDOW_ADDR 	0x50000000
#define BUSWIDTH 	2
/*  chanage it as following . lizhijie 2004.07.09
#define WINDOW_SIZE	0x01000000
*/
#define WINDOW_SIZE	0x0400000

#ifndef __ARMEB__
#define	B0(h)	((h) & 0xFF)
#define	B1(h)	(((h) >> 8) & 0xFF)
#else
#define	B0(h)	(((h) >> 8) & 0xFF)
#define	B1(h)	((h) & 0xFF)
#endif

static __u16 ixp425_read16(struct map_info *map, unsigned long ofs)
{
//	return readw(map->map_priv_1 + ofs);
	return *(__u16 *)(map->map_priv_1 + ofs);
}

/*
 * The IXP425 expansion bus only allows 16-bit wide acceses
 * when attached to a 16-bit wide device (such as the 28F128J3A),
 * so we can't use a memcpy_fromio as it does byte acceses.
 */
static void ixp425_copy_from(struct map_info *map, void *to,
    unsigned long from, ssize_t len)
{
	int i;
	u8 *dest = (u8*)to;
	u16 *src = (u16 *)(map->map_priv_1 + from);
	u16 data;

	for(i = 0; i < (len / 2); i++) {
		data = src[i];
		dest[i * 2] = B0(data);
		dest[i * 2 + 1] = B1(data);
	}

	if(len & 1)
		dest[len - 1] = B0(src[i]);
}

static void ixp425_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	*(__u16 *)(map->map_priv_1 + adr) = d;
}

static struct map_info ixp425_map = {
	name: 		"IXP422 Flash(TE28F320C3TC90)",
	buswidth: 	BUSWIDTH,
	read16:		ixp425_read16,
	copy_from:	ixp425_copy_from,
	write16:	ixp425_write16,
};

#ifdef CONFIG_MTD_REDBOOT_PARTS
static struct mtd_partition *parsed_parts;
#endif

/*
	release version is following
*/
static struct mtd_partition ixp425_partitions[] = {
	{/*u-boot occuppied the first 192k block(bootloader:128K, environment:64K )*/
		name:	"boot loader",
		offset:	0x00000000,
		size:	0x00030000, 
    	},

#if 0    	
/* commentted by lizhijie 2004.12.29 */
	{/* kernel image occupied the next 1024k block  + 64K */
		name:	"kernel image",
		offset:	0x00030000,
		size:	0x00110000, 
    	},
#endif

	{/* kernel image occupied the next 1024k block  + 512K = 1536 K 
	    Flash map for releases version, lizhjie 2004.12.29
	*/
		name:	"kernel image",
		offset:	0x00030000,
		size:	0x00180000, 
    	},
    	
#if 0    	
	{/* root file system occupied other space :2880k */
		name:	"init rd",
		offset:	0x00130000,
		size:	0x000D0000, 
	},
#endif	

#if 0
/* commentted by lizhijie 2004.12.29 */
	{/* root file system occupied other space : */
		name:	"jffs2 FS",
		offset:	0x00140000,
		size:	0x002C0000, 
	},
#endif
	{/* root file system occupied other space : 
	 2368 K ,except the first 192k, and 1536K occupied by kernel
	 Li zhijie 2004.12.29 
	*/
		name:	"jffs2 FS",
		offset:	0x001B0000,
		size:	0x00250000, 
	},
	
};

#define NB_OF(x)  (sizeof(x)/sizeof(x[0]))

static struct mtd_info *ixp425_mtd;
static struct resource *mtd_resource;

static void ixp425_exit(void)
{
    if(ixp425_mtd){
	del_mtd_partitions(ixp425_mtd);
	map_destroy(ixp425_mtd);
    }
    if (ixp425_map.map_priv_1)
	iounmap((void *)ixp425_map.map_priv_1);
    if (mtd_resource)
	release_mem_region(WINDOW_ADDR, WINDOW_SIZE);
    
#ifdef CONFIG_MTD_REDBOOT_PARTS
    if (parsed_parts)
	kfree(parsed_parts);
#endif

    /* Disable flash write */
    *IXP425_EXP_CS0 &= ~IXP425_FLASH_WRITABLE;
}

static int __init ixp425_init(void)
{
    int res = -1, npart;

    /* Enable flash write */
    *IXP425_EXP_CS0 |= IXP425_FLASH_WRITABLE;

//	printk("%s %s\n", __FILE__,  __FUNCTION__ );
    ixp425_map.map_priv_1 = 0;
    mtd_resource = 
	request_mem_region(WINDOW_ADDR, WINDOW_SIZE, "ixp425 Flash");
    if(!mtd_resource) {
	printk(KERN_ERR "ixp425 flash: Could not request mem region.\n" );
	res = -ENOMEM;
	goto Error;
    }

    ixp425_map.map_priv_1 =
	(unsigned long)ioremap(WINDOW_ADDR, WINDOW_SIZE);
    if (!ixp425_map.map_priv_1) {
	printk("ixp425 Flash: Failed to map IO region. (ioremap)\n");
	res = -EIO;
	goto Error;
    }
//	printk("ixp425_map->map_priv_1=%lx\n", ixp425_map.map_priv_1);

	ixp425_map.size = WINDOW_SIZE;


    /* 
     * Probe for the CFI complaint chip
     * suposed to be 28F128J3A
     */
    ixp425_mtd = do_map_probe("cfi_probe", &ixp425_map);
    if (!ixp425_mtd) {
	res = -ENXIO;
	goto Error;
    }
    ixp425_mtd->module = THIS_MODULE;
   
    /* Initialize flash partiotions 
     * Note: Redeboot partition info table can be parsed by MTD, and used
     *       instead of hard-coded partions. TBD
     */

#ifdef CONFIG_MTD_REDBOOT_PARTS
    /* Try to parse RedBoot partitions */
    npart = parse_redboot_partitions(ixp425_mtd, &parsed_parts);
    if (npart > 0) 
	res = add_mtd_partitions(ixp425_mtd, parsed_parts, npart);
    else   
	res = -EIO;
#endif

    if (res) {
	printk("Using static MTD partitions.\n");
	/* RedBoot partitions not found - use hardcoded partition table */
	res = add_mtd_partitions(ixp425_mtd, ixp425_partitions,
	    NB_OF(ixp425_partitions));
    }

    if (res)
	goto Error;

    return res;
Error:
    ixp425_exit();
    return res;
}

module_init(ixp425_init);
module_exit(ixp425_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTD map driver for ixp425 evaluation board");


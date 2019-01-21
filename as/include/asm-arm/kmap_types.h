#ifndef _ASM_KMAP_TYPES_H
#define _ASM_KMAP_TYPES_H
/*
* In order to use crypt API in IXP4xx, port this file from x386
* Li Zhijie, 2007.01.18
*/
enum km_type {
	KM_BOUNCE_READ,
	KM_SKB_SUNRPC_DATA,
	KM_SKB_DATA_SOFTIRQ,
	KM_USER0,
	KM_USER1,
	KM_BH_IRQ,
	KM_SOFTIRQ0,
	KM_SOFTIRQ1,
	KM_TYPE_NR
};

#endif

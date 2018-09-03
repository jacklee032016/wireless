/*****************************************************************************
 *  Copyright 2005, Univerity of Colorado at Boulder.                        *
 *                                                                           *
 *                        All Rights Reserved                                *
 *                                                                           *
 *  Permission to use, copy, modify, and distribute this software and its    *
 *  documentation for any purpose other than its incorporation into a        *
 *  commercial product is hereby granted without fee, provided that the      *
 *  above copyright notice appear in all copies and that both that           *
 *  copyright notice and this permission notice appear in supporting         *
 *  documentation, and that the name of the University not be used in        *
 *  advertising or publicity pertaining to distribution of the software      *
 *  without specific, written prior permission.                              *
 *                                                                           *
 *  UNIVERSITY OF COLORADO DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS      *
 *  SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND        *
 *  FITNESS FOR ANY PARTICULAR PURPOSE.  IN NO EVENT SHALL THE UNIVERSITY    *
 *  OF COLORADO BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL         *
 *  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA       *
 *  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER        *
 *  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR         *
 *  PERFORMANCE OF THIS SOFTWARE.                                            *
 *                                                                           * 
 ****************************************************************************/

/**
 * @file softmac_core.c
 *  SoftMAC Core Services
 */

#include <linux/module.h>
#if __ARM__
#include <asm/uaccess.h>
#else
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>

#include "cu_softmac_api.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeff Fifield");

/*
** List handling
*/

#if __ARM__
	#define MYLIST_HEAD				struct list_head
	#define MYLIST_NODE				struct list_head

	#define MYLIST_INIT_LIST_HEAD 	INIT_LIST_HEAD
	#define MYLIST_FOR_EACH		list_for_each
	#define MYLIST_ENTRY			list_entry
	#define MYLIST_ADD_HEAD		list_add
	#define MYLIST_DEL				list_del
#else
	#define MYLIST_HEAD				struct hlist_head
	#define MYLIST_NODE				struct hlist_node

	#define MYLIST_INIT_LIST_HEAD  	INIT_HLIST_HEAD
	#define MYLIST_FOR_EACH		hlist_for_each
	#define MYLIST_ENTRY			hlist_entry
	#define MYLIST_ADD_HEAD		hlist_add_head
	#define MYLIST_DEL				hlist_del
#endif


#define NAME_HASH_BITS 	8
static MYLIST_HEAD layer_name_head[1<<NAME_HASH_BITS];
static MYLIST_HEAD mac_name_head[1<<NAME_HASH_BITS];
static MYLIST_HEAD phy_name_head[1<<NAME_HASH_BITS];

// XXX should be locking the list for changes
//lock_t softmac_hlist_lock;

static inline MYLIST_HEAD *softmac_layer_hash(const char *name)
{
	unsigned hash = full_name_hash(name, strnlen(name, CU_SOFTMAC_NAME_SIZE));
	return &layer_name_head[hash & ((1 << NAME_HASH_BITS)-1)];
}

static inline MYLIST_HEAD *softmac_macinfo_hash(const char *name)
{
	unsigned hash = full_name_hash(name, strnlen(name, CU_SOFTMAC_NAME_SIZE));
	return &mac_name_head[hash & ((1 << NAME_HASH_BITS)-1)];
}

static inline MYLIST_HEAD *softmac_phyinfo_hash(const char *name)
{
	unsigned hash = full_name_hash(name, strnlen(name, CU_SOFTMAC_NAME_SIZE));
	return &phy_name_head[hash & ((1 << NAME_HASH_BITS)-1)];
}

static void softmac_lists_init(void)
{
	int i;
	
	for (i=0; i<ARRAY_SIZE(layer_name_head); i++)
		MYLIST_INIT_LIST_HEAD(&layer_name_head[i]);

	for (i=0; i<ARRAY_SIZE(mac_name_head); i++)
		MYLIST_INIT_LIST_HEAD(&mac_name_head[i]);

	for (i=0; i<ARRAY_SIZE(phy_name_head); i++)
		MYLIST_INIT_LIST_HEAD(&phy_name_head[i]);

}

/*
** Proc filesystem
*/

/**
 *  Information about each /proc/softmac entry for SoftMAC parameters
 */
typedef struct
{
	/**  Name that will appear in proc directory   */
	const char* name;
	/***  Read/write permissions, e.g. "0644" means read/write
	* for owner (root), and read-only for group and other users.   */

	mode_t mode;
	/***  A unique ID passed to the read and write handler functions
	* when data is written to or read from the particular entry.   */
	int id;
} SOFTMAC_PROC_ENTRY;

/* proc entry ID's */
enum
{
	SOFTMAC_PROC_CREATE_INSTANCE,
	SOFTMAC_PROC_DELETE_INSTANCE,
	SOFTMAC_PROC_CHEESYATH_START,
	SOFTMAC_PROC_CHEESYATH_STOP,
	SOFTMAC_PROC_TEST,
};

static const SOFTMAC_PROC_ENTRY softmac_proc_entries[] = 
{
	{
		"create_instance",
		0200,
		SOFTMAC_PROC_CREATE_INSTANCE
	},
	{
		"delete_instance",
		0200,
		SOFTMAC_PROC_DELETE_INSTANCE
	},
	{
		"cheesyath_start",
		0200,
		SOFTMAC_PROC_CHEESYATH_START
	},
	{
		"cheesyath_stop",
		0200,
		SOFTMAC_PROC_CHEESYATH_STOP
	},
	{
		"test",
		0200,
		SOFTMAC_PROC_TEST
	},

	/* the terminator */
	{
		0,
		0,
		-1
	}
};

/* data passed to the softmac_procfs_read function */
typedef struct
{
	char name[CU_SOFTMAC_NAME_SIZE];
	int id;
} SOFTMAC_PROC_DATA;

static struct proc_dir_entry *softmac_procfs_root;
static struct proc_dir_entry *softmac_procfs_layers;
static struct proc_dir_entry *softmac_procfs_insts;

static int softmac_procfs_read(char *page, char **start, off_t off, int count, int *eof, void *data);
static int softmac_procfs_write(struct file *file, const char __user *buffer,unsigned long count, void *data);

static void softmac_procfs_create(void)
{
	int i;
	struct proc_dir_entry *ent;
	SOFTMAC_PROC_DATA *data;

	/* add /proc/softmac/ directory */
	softmac_procfs_root = proc_mkdir("softmac", 0);
	softmac_procfs_root->owner = THIS_MODULE;

	/* add /proc/softmac/layers/ directory */
	softmac_procfs_layers = proc_mkdir("layers", softmac_procfs_root);
	softmac_procfs_layers->owner = THIS_MODULE;

	/* add /proc/softmac/insts/ directory */
	softmac_procfs_insts = proc_mkdir("insts", softmac_procfs_root);
	softmac_procfs_insts->owner = THIS_MODULE;

	/* add /proc/softmac entries */
	/* stop with null or empty string for name */
	for (i=0; softmac_proc_entries[i].name && softmac_proc_entries[i].name[0]; i++)
	{
		/* make a new entry */
		ent = create_proc_entry(softmac_proc_entries[i].name, softmac_proc_entries[i].mode, softmac_procfs_root);
		ent->owner = THIS_MODULE;
		ent->read_proc = softmac_procfs_read;
		ent->write_proc = softmac_procfs_write;

		data = kmalloc(sizeof(SOFTMAC_PROC_DATA), GFP_ATOMIC);
		data->id = softmac_proc_entries[i].id;
		strncpy(data->name, softmac_proc_entries[i].name, CU_SOFTMAC_NAME_SIZE);
		ent->data = data;
	}
}

static void softmac_procfs_destroy(void)
{
	int i;

	/* remove /proc/softmac/class/ directory */
	remove_proc_entry("layers", softmac_procfs_root);

	/* remove /proc/softmac/inst/ directory */
	remove_proc_entry("insts", softmac_procfs_root);

	/* remove /proc/softmac/ entries */
	for (i=0; softmac_proc_entries[i].name && softmac_proc_entries[i].name[0]; i++)
	{
		remove_proc_entry(softmac_proc_entries[i].name, softmac_procfs_root);
	}

	/* remove /proc/softmac/ directory */
	remove_proc_entry("softmac", 0);
}

/* functions for /proc/softmac */

static int softmac_procfs_read(char *page, char **start, off_t off,  int count, int *eof, void *data)
{
	return 0;
}

static void softmac_test(void);
static void softmac_cheesyath_start(void);
static void softmac_cheesyath_stop(void);

static int softmac_procfs_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int result = 0;
	char kdata[CU_SOFTMAC_NAME_SIZE];
	SOFTMAC_PROC_DATA *procdata = data;
	void *inst;

	if (count >= CU_SOFTMAC_NAME_SIZE)
		result = CU_SOFTMAC_NAME_SIZE-1;
	else 
		result = count-1;

	copy_from_user(kdata, buffer, result);
	kdata[result] = 0;

	switch (procdata->id)
	{
		case SOFTMAC_PROC_CREATE_INSTANCE:
			inst = cu_softmac_layer_new_instance(kdata);
			break;

		case SOFTMAC_PROC_DELETE_INSTANCE:
			inst = cu_softmac_macinfo_get_by_name(kdata);
			if (inst)
			{
				CU_SOFTMAC_MACLAYER_INFO *macinfo = inst;
				/* free the reference just aquired */
				cu_softmac_macinfo_free(macinfo);
				/* remove from SoftMAC */
				cu_softmac_macinfo_unregister(macinfo);
				break;
			}

			inst = cu_softmac_phyinfo_get_by_name(kdata);
			if (inst)
			{
				CU_SOFTMAC_PHYLAYER_INFO *phyinfo = inst;
				/* free the reference just aquired */
				cu_softmac_phyinfo_free(phyinfo);
				/* remove from SoftMAC */
				cu_softmac_phyinfo_unregister(phyinfo);
				break;
			}
			break;

		case SOFTMAC_PROC_CHEESYATH_START:
			softmac_cheesyath_start();
			break;

		case SOFTMAC_PROC_CHEESYATH_STOP:
			softmac_cheesyath_stop();
			break;

		case SOFTMAC_PROC_TEST:
			softmac_test();
			break;

		default:
			break;
    }

    return count;
}

/* functions for /proc/softmac/layers */

static void* softmac_procfs_layer_add(CU_SOFTMAC_LAYER_INFO *info)
{
	struct proc_dir_entry* ent;
	ent = proc_mkdir(info->name, softmac_procfs_layers);
	ent->owner = THIS_MODULE;
	return (void *)ent;
}

static void softmac_procfs_layer_del(CU_SOFTMAC_LAYER_INFO *info)
{
	remove_proc_entry(info->name, softmac_procfs_layers);
}

/* functions for /proc/softmac/insts */

static void *softmac_procfs_inst_add(void *inst)
{
	CU_SOFTMAC_MACLAYER_INFO *macinfo = inst;

	struct proc_dir_entry* ent;
	ent = proc_mkdir(macinfo->name, softmac_procfs_insts);
	ent->owner = THIS_MODULE;
	return (void *)ent;
}

static void softmac_procfs_inst_del(void *inst)
{
	CU_SOFTMAC_MACLAYER_INFO *macinfo = inst;
	remove_proc_entry(macinfo->name, softmac_procfs_insts);
}

/* Implementations of our "do nothing" functions to avoid null checks */

int cu_softmac_phy_attach_dummy(void *mydata, struct CU_SOFTMAC_MACLAYER_INFO_t* macinfo) 
{
	return -1;
}

void cu_softmac_phy_detach_dummy(void *mydata) 
{
}

u_int64_t cu_softmac_phy_get_time_dummy(void *mydata) 
{
	return 0;
}

void cu_softmac_phy_set_time_dummy(void *mydata, u_int64_t time) 
{
}

void cu_softmac_phy_schedule_work_asap_dummy(void *mydata) 
{
}

struct sk_buff* cu_softmac_phy_alloc_skb_dummy(void *mydata, int datalen) 
{
	return 0;
}

void cu_softmac_phy_free_skb_dummy(void *mydata, struct sk_buff* skb)
{
	/*
	* Free the packet if it's not null -- not technically "nothing" but
	* may prevent some memory leakage in corner cases.     */
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
}

int cu_softmac_phy_sendpacket_dummy(void *mydata, int max_packets_inflight, struct sk_buff* skb) 
{
	/** Free the packet if it's not null -- not technically "nothing" but
	* may prevent some memory leakage in corner cases.     */
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return CU_SOFTMAC_PHY_SENDPACKET_OK;
}

int cu_softmac_phy_sendpacket_keepskbonfail_dummy(void *mydata,
					      int max_packets_inflight, struct sk_buff* skb) 
{
	/** Free the packet if it's not null -- not technically "nothing" but
	* may prevent some memory leakage in corner cases.     */
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return CU_SOFTMAC_PHY_SENDPACKET_OK;
}

u_int32_t cu_softmac_phy_get_duration_dummy(void *mydata,struct sk_buff* skb) 
{ 
	return 0; 
}

u_int32_t cu_softmac_phy_get_txlatency_dummy(void *mydata) 
{
	return 0;
}

static int cu_softmac_mac_work_dummy(void *mydata, int intop)
{
	return 0;
}

static int cu_softmac_mac_packet_rx_dummy(void *mydata, struct sk_buff* skb, int intop)
{
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return CU_SOFTMAC_MAC_NOTIFY_OK;
}

static int cu_softmac_mac_packet_tx_done_dummy(void *mydata, struct sk_buff* skb, int intop)
{
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return CU_SOFTMAC_MAC_NOTIFY_OK;
}

static int cu_softmac_mac_packet_tx_dummy(void *mydata, struct sk_buff* skb, int intop)
{
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return CU_SOFTMAC_MAC_NOTIFY_OK;
}

static int cu_softmac_mac_attach_dummy(void *mydata, CU_SOFTMAC_PHYLAYER_INFO* phyinfo)
{
	return -1;
}

static int cu_softmac_mac_detach_dummy(void *mydata)
{
	return 0;
}

static int cu_softmac_mac_set_rx_func_dummy(void *mydata, CU_SOFTMAC_MAC_RX_FUNC rxfunc, void *rxpriv)
{
	return 0;
}

static int cu_softmac_mac_set_unload_notify_func_dummy(void *mydata,CU_SOFTMAC_MAC_UNLOAD_NOTIFY_FUNC unloadfunc, void *unloadpriv)
{
	return 0;
}


/*
** SoftMAC Core API - public interface
*/

/* returns CU_SOFTMAC_MACLAYER_INFO or CU_SOFTMAC_PHYLAYER_INFO cast to void **/
void *cu_softmac_layer_new_instance(const char *name)
{
	//printk("%s\n", __func__);
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	head = softmac_layer_hash(name);
	MYLIST_FOR_EACH(p, head)
	{
		CU_SOFTMAC_LAYER_INFO *l = MYLIST_ENTRY(p, CU_SOFTMAC_LAYER_INFO, name_hlist);
		if (!strncmp(l->name, name, CU_SOFTMAC_NAME_SIZE))
			return (l->cu_softmac_layer_new_instance)(l->layer_private);
	}
	return 0;
}

void cu_softmac_layer_free_instance(void *inst)
{
	//printk("%s\n", __func__);
	CU_SOFTMAC_LAYER_INFO *l;

	/* XXX ugly! */
	l = ((CU_SOFTMAC_MACLAYER_INFO *)(inst))->layer;
	l->cu_softmac_layer_free_instance(l->layer_private, inst);
}


void cu_softmac_layer_register(CU_SOFTMAC_LAYER_INFO *info)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	/* check for existing */
	head = softmac_layer_hash(info->name);
	MYLIST_FOR_EACH(p, head)
	{
		CU_SOFTMAC_LAYER_INFO *l = MYLIST_ENTRY(p, CU_SOFTMAC_LAYER_INFO, name_hlist);
		if (!strncmp(l->name, info->name, CU_SOFTMAC_NAME_SIZE))
		{
			printk("%s error: layer %s already registered\n", __func__, info->name);
			return;
		}
	}

	/* add it to /proc/softmac/layers and to internal list */
	info->proc = softmac_procfs_layer_add(info);
	MYLIST_ADD_HEAD(&info->name_hlist, head);

	printk("%s registered layer %s\n", __func__, info->name);
}

void cu_softmac_layer_unregister(CU_SOFTMAC_LAYER_INFO *info)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	head = softmac_layer_hash(info->name);
	MYLIST_FOR_EACH(p, head)
	{
		CU_SOFTMAC_LAYER_INFO *l = MYLIST_ENTRY(p, CU_SOFTMAC_LAYER_INFO, name_hlist);
		if (l == info)
		{
			printk("%s unregistered layer %s\n", __func__, info->name);
			/* remove from procfs */
			softmac_procfs_layer_del(info);
			info->proc = 0;
			/* remove from internal list */
			MYLIST_DEL(&l->name_hlist);
			return;
		}
	}
}

CU_SOFTMAC_PHYLAYER_INFO *cu_softmac_phyinfo_alloc(void)
{
	//printk("%s\n", __func__);
	CU_SOFTMAC_PHYLAYER_INFO *phyinfo;

	phyinfo = kmalloc(sizeof(CU_SOFTMAC_PHYLAYER_INFO) ,GFP_ATOMIC);
	if (phyinfo)
	{
		cu_softmac_phyinfo_init(phyinfo);
		atomic_set (&phyinfo->refcnt, 1);
	}

	return phyinfo;
}

void cu_softmac_phyinfo_free(CU_SOFTMAC_PHYLAYER_INFO *phyinfo)
{
	//printk("%s\n", __func__);
	if (phyinfo && atomic_dec_and_test(&phyinfo->refcnt))
	{
		printk("%s freed %s\n", __func__, phyinfo->name);
		if (phyinfo->phy_private)
			cu_softmac_layer_free_instance(phyinfo);
		kfree(phyinfo);
	}
}

void cu_softmac_phyinfo_init(CU_SOFTMAC_PHYLAYER_INFO* phyinfo)
{
	//printk("%s\n", __func__);

	memset(phyinfo, 0, sizeof(CU_SOFTMAC_PHYLAYER_INFO));
	phyinfo->cu_softmac_phy_attach = cu_softmac_phy_attach_dummy;
	phyinfo->cu_softmac_phy_detach = cu_softmac_phy_detach_dummy;
	phyinfo->cu_softmac_phy_get_time = cu_softmac_phy_get_time_dummy;
	phyinfo->cu_softmac_phy_set_time = cu_softmac_phy_set_time_dummy;
	phyinfo->cu_softmac_phy_schedule_work_asap = cu_softmac_phy_schedule_work_asap_dummy;
	phyinfo->cu_softmac_phy_alloc_skb = cu_softmac_phy_alloc_skb_dummy;
	phyinfo->cu_softmac_phy_free_skb = cu_softmac_phy_free_skb_dummy;
	phyinfo->cu_softmac_phy_sendpacket = cu_softmac_phy_sendpacket_dummy;
	phyinfo->cu_softmac_phy_sendpacket_keepskbonfail=cu_softmac_phy_sendpacket_keepskbonfail_dummy;
	phyinfo->cu_softmac_phy_get_duration = cu_softmac_phy_get_duration_dummy;
	phyinfo->cu_softmac_phy_get_txlatency = cu_softmac_phy_get_txlatency_dummy;
}

void cu_softmac_phyinfo_register(CU_SOFTMAC_PHYLAYER_INFO* phyinfo)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	/* check for existing */
	head = softmac_phyinfo_hash(phyinfo->name);
	MYLIST_FOR_EACH(p, head)
	{
		CU_SOFTMAC_PHYLAYER_INFO *m = MYLIST_ENTRY(p, CU_SOFTMAC_PHYLAYER_INFO, name_hlist);
		if (!strncmp(m->name, phyinfo->name, CU_SOFTMAC_NAME_SIZE))
		{
			printk("%s error: phy %s already registered\n", __func__, m->name);
			return;
		}
	}

	/* increment reference count */
	phyinfo = cu_softmac_phyinfo_get(phyinfo);
	/* add to /proc/softmac/insts */
	phyinfo->proc = softmac_procfs_inst_add(phyinfo);
	/* add to the phyinfo list */
	MYLIST_ADD_HEAD(&phyinfo->name_hlist, head);

	printk("%s registered %s\n", __func__, phyinfo->name);
}

void cu_softmac_phyinfo_unregister(CU_SOFTMAC_PHYLAYER_INFO* phyinfo)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	head = softmac_phyinfo_hash(phyinfo->name);
	MYLIST_FOR_EACH(p, head)
	{
		CU_SOFTMAC_PHYLAYER_INFO *p = MYLIST_ENTRY(p, CU_SOFTMAC_PHYLAYER_INFO, name_hlist);
		if (p == phyinfo)
		{
			/* save for removal from /proc/softmac/insts 
			* the directory cannot be removed before cu_softmac_phyinfo_free */
			CU_SOFTMAC_MACLAYER_INFO *tmp = kmalloc(sizeof(CU_SOFTMAC_MACLAYER_INFO), GFP_ATOMIC);
			strncpy(tmp->name, p->name, CU_SOFTMAC_NAME_SIZE);

			/* remove from internal pyinfo list */
			MYLIST_DEL(&p->name_hlist);
			/* decrement reference count */
			cu_softmac_phyinfo_free(p);
			/* remove from proc */
			softmac_procfs_inst_del(tmp);
			kfree(tmp);
			printk("%s unregistered %s\n", __func__, phyinfo->name);
			return;
		}
	}
}

CU_SOFTMAC_PHYLAYER_INFO *cu_softmac_phyinfo_get(CU_SOFTMAC_PHYLAYER_INFO* phyinfo)
{
	//printk("%s\n", __func__);
	if (phyinfo)
		atomic_inc(&phyinfo->refcnt);
	return phyinfo;
}

CU_SOFTMAC_PHYLAYER_INFO *cu_softmac_phyinfo_get_by_name(const char *name)
{
	//printk("%s\n", __func__);
	MYLIST_HEAD *head;
	MYLIST_NODE *p;
	CU_SOFTMAC_PHYLAYER_INFO *ret = 0;

	head = softmac_phyinfo_hash(name);
	MYLIST_FOR_EACH(p, head)
	{
		CU_SOFTMAC_PHYLAYER_INFO *m = MYLIST_ENTRY(p, CU_SOFTMAC_PHYLAYER_INFO, name_hlist);
		if (!strncmp(m->name, name, CU_SOFTMAC_NAME_SIZE))
			ret = m;
	}

	if (ret)
		atomic_inc(&ret->refcnt);

	return ret;
}

CU_SOFTMAC_MACLAYER_INFO *cu_softmac_macinfo_alloc(void)
{
	//printk("%s\n", __func__);
	CU_SOFTMAC_MACLAYER_INFO *macinfo;
	macinfo = kmalloc(sizeof(CU_SOFTMAC_MACLAYER_INFO),GFP_ATOMIC);
	if (macinfo)
	{
		cu_softmac_macinfo_init(macinfo);
		atomic_set(&macinfo->refcnt, 1);
	}
	return macinfo;
}

void cu_softmac_macinfo_free(CU_SOFTMAC_MACLAYER_INFO *macinfo)
{
	//printk("%s\n", __func__);

	if (macinfo && atomic_dec_and_test(&macinfo->refcnt))
	{
		printk("%s freed %s\n", __func__, macinfo->name);
		if (macinfo->mac_private)
			cu_softmac_layer_free_instance(macinfo);
		kfree(macinfo);
	}
}

void cu_softmac_macinfo_init(CU_SOFTMAC_MACLAYER_INFO* macinfo)
{
	//printk("%s\n", __func__);

	memset(macinfo, 0, sizeof(CU_SOFTMAC_MACLAYER_INFO));
	macinfo->cu_softmac_mac_packet_tx = cu_softmac_mac_packet_tx_dummy;
	macinfo->cu_softmac_mac_packet_tx_done = cu_softmac_mac_packet_tx_done_dummy;
	macinfo->cu_softmac_mac_packet_rx = cu_softmac_mac_packet_rx_dummy;
	macinfo->cu_softmac_mac_work = cu_softmac_mac_work_dummy;
	macinfo->cu_softmac_mac_attach = cu_softmac_mac_attach_dummy;
	macinfo->cu_softmac_mac_detach = cu_softmac_mac_detach_dummy;
	macinfo->cu_softmac_mac_set_rx_func = cu_softmac_mac_set_rx_func_dummy;
	macinfo->cu_softmac_mac_set_unload_notify_func = cu_softmac_mac_set_unload_notify_func_dummy;
}

void cu_softmac_macinfo_register(CU_SOFTMAC_MACLAYER_INFO* macinfo)
{
	//printk("%s\n", __func__);
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	/* check for existing */
	head = softmac_macinfo_hash(macinfo->name);
	MYLIST_FOR_EACH(p, head)
	{
		CU_SOFTMAC_MACLAYER_INFO *m = MYLIST_ENTRY(p, CU_SOFTMAC_MACLAYER_INFO, name_hlist);
		if (!strncmp(m->name, macinfo->name, CU_SOFTMAC_NAME_SIZE))
		{
			printk("%s error: mac %s already registered\n", __func__, m->name);
			return;
		}
	}

	/* increment reference count */
	macinfo = cu_softmac_macinfo_get(macinfo);
	/* add to /proc/softmac/insts */
	macinfo->proc = softmac_procfs_inst_add(macinfo);
	/* add to the macinfo list */
	MYLIST_ADD_HEAD(&macinfo->name_hlist, head);

	printk("%s registered %s\n", __func__, macinfo->name);
}

void cu_softmac_macinfo_unregister(CU_SOFTMAC_MACLAYER_INFO* macinfo)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	head = softmac_macinfo_hash(macinfo->name);
	MYLIST_FOR_EACH(p, head)
	{
		CU_SOFTMAC_MACLAYER_INFO *m = MYLIST_ENTRY(p, CU_SOFTMAC_MACLAYER_INFO, name_hlist);
		if (m == macinfo)
		{
			/* save for removal from /proc/softmac/insts 
			* the directory cannot be removed before cu_softmac_macinfo_free */
			CU_SOFTMAC_MACLAYER_INFO *tmp = kmalloc(sizeof(CU_SOFTMAC_MACLAYER_INFO), GFP_ATOMIC);
			strncpy(tmp->name, m->name, CU_SOFTMAC_NAME_SIZE);

			/* remove from internal list */
			MYLIST_DEL(&m->name_hlist);
			/* decrement reference count */
			cu_softmac_macinfo_free(m);

			/* remove from proc */
			softmac_procfs_inst_del(tmp);
			kfree(tmp);

			printk("%s unregistered %s\n", __func__, macinfo->name);
			return;
		}
	}
}

CU_SOFTMAC_MACLAYER_INFO *cu_softmac_macinfo_get(CU_SOFTMAC_MACLAYER_INFO *macinfo)
{
	//printk("%s\n", __func__);
	if (macinfo)
		atomic_inc(&macinfo->refcnt);
	return macinfo;
}

CU_SOFTMAC_MACLAYER_INFO *cu_softmac_macinfo_get_by_name(const char *name)
{
	//printk("%s\n", __func__);
	MYLIST_HEAD *head;
	MYLIST_NODE *p;
	CU_SOFTMAC_MACLAYER_INFO *ret = 0;

	head = softmac_macinfo_hash(name);
	MYLIST_FOR_EACH(p, head)
	{
		CU_SOFTMAC_MACLAYER_INFO *m = MYLIST_ENTRY(p, CU_SOFTMAC_MACLAYER_INFO, name_hlist);
		if (!strncmp(m->name, name, CU_SOFTMAC_NAME_SIZE))
			ret = m;
	}

	if (ret)
		atomic_inc(&ret->refcnt);

	return ret;
}

/* XXX start test code */
static CU_SOFTMAC_MACLAYER_INFO *testmac;
static CU_SOFTMAC_PHYLAYER_INFO *testphy;

static void softmac_cheesyath_start()
{
	if (testmac || testphy)
	{
		printk("%s cheesymac or athphy instance already exists\n", __func__);
		return;
	}

	testmac = cu_softmac_layer_new_instance("cheesymac");
	testphy = cu_softmac_layer_new_instance("athphy");

	if (!(testmac && testphy))
	{
		printk("%s failed to create cheesymac or athphy instance\n", __func__);
		return;
	}

	cu_softmac_macinfo_get(testmac);
	cu_softmac_phyinfo_get(testphy);

	testmac->cu_softmac_mac_attach(testmac->mac_private, testphy);
	testphy->cu_softmac_phy_attach(testphy->phy_private, testmac);

	printk("%s cheesyath started\n", __func__);
}

static void softmac_cheesyath_stop()
{
	if (!(testmac && testphy))
	{
		printk("%s cheesyath not active\n", __func__);
		return;
	}

	testmac->cu_softmac_mac_detach(testmac->mac_private);
	testphy->cu_softmac_phy_detach(testphy->phy_private);

	cu_softmac_macinfo_free(testmac);
	cu_softmac_phyinfo_free(testphy);

	testmac = 0;
	testphy = 0;
	printk("%s cheesyath stopped\n", __func__);
}

static void softmac_test(void)
{
	printk("%s\n", __func__);

	if (testmac || testphy)
	{
		printk("%s cheesymac or athphy instance already exists\n", __func__);
		return;
	}

	testmac = cu_softmac_layer_new_instance("rawmac");
	testphy = cu_softmac_layer_new_instance("athphy");

	if (!(testmac && testphy))
	{
		printk("%s failed to create rawmac or athphy instance\n", __func__);
		return;
	}

	cu_softmac_macinfo_get(testmac);
	cu_softmac_phyinfo_get(testphy);

	testmac->cu_softmac_mac_attach(testmac->mac_private, testphy);
	testphy->cu_softmac_phy_attach(testphy->phy_private, testmac);

	printk("%s started\n", __func__);
}
/* XXX end test code */

static int __init softmac_core_init(void)
{
	softmac_lists_init();
	softmac_procfs_create();

	return 0;
}

static void __exit softmac_core_exit(void)
{
	printk("%s\n", __func__);

	//softmac_lists_cleanup();
	softmac_procfs_destroy();
}

module_init(softmac_core_init);
module_exit(softmac_core_exit);

EXPORT_SYMBOL(cu_softmac_layer_register);
EXPORT_SYMBOL(cu_softmac_layer_unregister);
EXPORT_SYMBOL(cu_softmac_layer_new_instance);
EXPORT_SYMBOL(cu_softmac_layer_free_instance );

EXPORT_SYMBOL(cu_softmac_phyinfo_register);
EXPORT_SYMBOL(cu_softmac_phyinfo_unregister);
EXPORT_SYMBOL(cu_softmac_phyinfo_get);
EXPORT_SYMBOL(cu_softmac_phyinfo_get_by_name);
EXPORT_SYMBOL(cu_softmac_phyinfo_alloc);
EXPORT_SYMBOL(cu_softmac_phyinfo_free);
EXPORT_SYMBOL(cu_softmac_phyinfo_init);

EXPORT_SYMBOL(cu_softmac_macinfo_register);
EXPORT_SYMBOL(cu_softmac_macinfo_unregister);
EXPORT_SYMBOL(cu_softmac_macinfo_get);
EXPORT_SYMBOL(cu_softmac_macinfo_get_by_name);
EXPORT_SYMBOL(cu_softmac_macinfo_alloc);
EXPORT_SYMBOL(cu_softmac_macinfo_free);
EXPORT_SYMBOL(cu_softmac_macinfo_init);


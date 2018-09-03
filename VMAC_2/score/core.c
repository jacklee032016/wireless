/*
 * $Id: core.c,v 1.1.1.1 2006/11/30 17:01:32 lizhijie Exp $
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

#include "vmac.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Li Zhijie");

#define DIR_CORE_NAME			"vmac"
#define DIR_LAYER_NAME			"layer"
#define DIR_INST_NAME			"inst"

#define NAME_HASH_BITS 		8

static MYLIST_HEAD 	layer_name_head[1<<NAME_HASH_BITS];
static MYLIST_HEAD 	mac_name_head[1<<NAME_HASH_BITS];
static MYLIST_HEAD 	phy_name_head[1<<NAME_HASH_BITS];

// XXX should be locking the list for changes
//lock_t softmac_hlist_lock;

static inline MYLIST_HEAD *__layer_hash(const char *name)
{
	unsigned hash = full_name_hash(name, strnlen(name, VMAC_NAME_SIZE));
	return &layer_name_head[hash & ((1 << NAME_HASH_BITS)-1)];
}

static inline MYLIST_HEAD *__macinfo_hash(const char *name)
{
	unsigned hash = full_name_hash(name, strnlen(name, VMAC_NAME_SIZE));
	return &mac_name_head[hash & ((1 << NAME_HASH_BITS)-1)];
}

static inline MYLIST_HEAD *__phyinfo_hash(const char *name)
{
	unsigned hash = full_name_hash(name, strnlen(name, VMAC_NAME_SIZE));
	return &phy_name_head[hash & ((1 << NAME_HASH_BITS)-1)];
}

static void __lists_init(void)
{
	int i;
	
	for (i=0; i<ARRAY_SIZE(layer_name_head); i++)
		MYLIST_INIT_LIST_HEAD(&layer_name_head[i]);

	for (i=0; i<ARRAY_SIZE(mac_name_head); i++)
		MYLIST_INIT_LIST_HEAD(&mac_name_head[i]);

	for (i=0; i<ARRAY_SIZE(phy_name_head); i++)
		MYLIST_INIT_LIST_HEAD(&phy_name_head[i]);

}

/* Proc filesystem */

/**
 *  Information about each /proc/softmac entry for VMAC parameters
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
} vmac_proc_entry_t;

/* proc entry ID's */
enum
{
	PROC_CMD_CREATE_INSTANCE,
	PROC_CMD_DELETE_INSTANCE,
	PROC_CMD_START,
	PROC_CMD_STOP,
	PROC_CMD_TEST,
};

static const vmac_proc_entry_t softmac_proc_entries[] = 
{
	{
		"create",
		0200,
		PROC_CMD_CREATE_INSTANCE
	},
	{
		"delete",
		0200,
		PROC_CMD_DELETE_INSTANCE
	},
	{
		"start",
		0200,
		PROC_CMD_START
	},
	{
		"stop",
		0200,
		PROC_CMD_STOP
	},
	{
		"test",
		0200,
		PROC_CMD_TEST
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
	char name[VMAC_NAME_SIZE];
	int id;
} vmac_proc_data_t;

static struct proc_dir_entry *softmac_procfs_root;
static struct proc_dir_entry *softmac_procfs_layers;
static struct proc_dir_entry *softmac_procfs_insts;


/* XXX start test code */
static vmac_mac_t *testmac;
static vmac_phy_t *testphy;

static void __cheesyath_start(void)
{
	if (testmac || testphy)
	{
		printk("%s cheesymac or athphy instance already exists\n", __func__);
		return;
	}

	testmac = vmac_layer_new_instance(VMAC_MODULE_ALOHA );
	testphy = vmac_layer_new_instance("athphy");

	if (!(testmac && testphy))
	{
		printk("%s failed to create cheesymac or athphy instance\n", __func__);
		return;
	}

	vmac_get_mac(testmac);
	vmac_get_phy(testphy);

	testmac->attach_phy(testmac->mac_private, testphy);
	testphy->attach_mac(testphy->phy_private, testmac);

	printk("%s cheesyath started\n", __func__);
}

static void __cheesyath_stop(void)
{
	if (!(testmac && testphy))
	{
		printk("%s cheesyath not active\n", __func__);
		return;
	}

	testmac->detach_phy(testmac->mac_private);
	testphy->detach_mac(testphy->phy_private);

	vmac_free_mac(testmac);
	vmac_free_phy(testphy);

	testmac = 0;
	testphy = 0;
	printk("%s cheesyath stopped\n", __func__);
}

static void __test(void)
{
	trace;
	
	if (testmac || testphy)
	{
		printk("%s cheesymac or athphy instance already exists\n", __func__);
		return;
	}

	testmac = vmac_layer_new_instance("rawmac");
	testphy = vmac_layer_new_instance("athphy");

	if (!(testmac && testphy))
	{
		printk("%s failed to create rawmac or athphy instance\n", __func__);
		return;
	}

	vmac_get_mac(testmac);
	vmac_get_phy(testphy);

	testmac->attach_phy(testmac->mac_private, testphy);
	testphy->attach_mac(testphy->phy_private, testmac);

	printk("%s started\n", __func__);
}
/* XXX end test code */

static int __procfs_write(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int result = 0;
	char kdata[VMAC_NAME_SIZE];
	vmac_proc_data_t *procdata = data;
	void *inst;

	if (count >= VMAC_NAME_SIZE)
		result = VMAC_NAME_SIZE-1;
	else 
		result = count-1;

	copy_from_user(kdata, buffer, result);
	kdata[result] = 0;

	switch (procdata->id)
	{
		case PROC_CMD_CREATE_INSTANCE:
			inst = vmac_layer_new_instance(kdata);
			break;

		case PROC_CMD_DELETE_INSTANCE:
			inst = vmac_get_mac_by_name(kdata);
			if (inst)
			{
				vmac_mac_t *macinfo = inst;
				/* free the reference just aquired */
				vmac_free_mac(macinfo);
				/* remove from VMAC */
				vmac_unregister_mac(macinfo);
				break;
			}

			inst = vmac_get_phy_by_name(kdata);
			if (inst)
			{
				vmac_phy_t *phyinfo = inst;
				/* free the reference just aquired */
				vmac_free_phy(phyinfo);
				/* remove from VMAC */
				vmac_unregister_phy(phyinfo);
				break;
			}
			break;

		case PROC_CMD_START:
			__cheesyath_start();
			break;

		case PROC_CMD_STOP:
			__cheesyath_stop();
			break;

		case PROC_CMD_TEST:
			__test();
			break;

		default:
			break;
    }

    return count;
}

/* functions for /proc/softmac */
static int __procfs_read(char *page, char **start, off_t off,  int count, int *eof, void *data)
{
	return 0;
}

static void __procfs_create(void)
{
	int i;
	struct proc_dir_entry *ent;
	vmac_proc_data_t *data;

	softmac_procfs_root = proc_mkdir(DIR_CORE_NAME, 0);
	softmac_procfs_root->owner = THIS_MODULE;

	softmac_procfs_layers = proc_mkdir(DIR_LAYER_NAME, softmac_procfs_root);
	softmac_procfs_layers->owner = THIS_MODULE;

	softmac_procfs_insts = proc_mkdir(DIR_INST_NAME, softmac_procfs_root);
	softmac_procfs_insts->owner = THIS_MODULE;

	/* stop with null or empty string for name */
	for (i=0; softmac_proc_entries[i].name && softmac_proc_entries[i].name[0]; i++)
	{
		/* make a new entry */
		ent = create_proc_entry(softmac_proc_entries[i].name, softmac_proc_entries[i].mode, softmac_procfs_root);
		ent->owner = THIS_MODULE;
		ent->read_proc = __procfs_read;
		ent->write_proc = __procfs_write;

		data = kmalloc(sizeof(vmac_proc_data_t), GFP_ATOMIC);
		data->id = softmac_proc_entries[i].id;
		strncpy(data->name, softmac_proc_entries[i].name, VMAC_NAME_SIZE);
		ent->data = data;
	}
}

static void __procfs_destroy(void)
{
	int i;

	remove_proc_entry( DIR_LAYER_NAME, softmac_procfs_root);

	remove_proc_entry( DIR_INST_NAME, softmac_procfs_root);

	for (i=0; softmac_proc_entries[i].name && softmac_proc_entries[i].name[0]; i++)
	{
		remove_proc_entry(softmac_proc_entries[i].name, softmac_procfs_root);
	}

	remove_proc_entry(DIR_CORE_NAME, 0);
}


static void* __procfs_layer_add(vmac_layer_t *info)
{
	struct proc_dir_entry* ent;
	ent = proc_mkdir(info->name, softmac_procfs_layers);
	ent->owner = THIS_MODULE;
	return (void *)ent;
}

static void __procfs_layer_del(vmac_layer_t *info)
{
	remove_proc_entry(info->name, softmac_procfs_layers);
}

/* functions for /proc/softmac/insts */
static void *__procfs_inst_add(void *inst)
{
	vmac_mac_t *macinfo = inst;

	struct proc_dir_entry* ent;
	ent = proc_mkdir(macinfo->name, softmac_procfs_insts);
	ent->owner = THIS_MODULE;
	return (void *)ent;
}

static void __procfs_inst_del(void *inst)
{
	vmac_mac_t *macinfo = inst;
	remove_proc_entry(macinfo->name, softmac_procfs_insts);
}

/* Implementations of our "do nothing" functions to avoid null checks */
static int __phy_attach_dummy(void *mydata, vmac_mac_t* macinfo) 
{
	return -1;
}

static void __phy_detach_dummy(void *mydata) 
{
}

static u_int64_t __phy_get_time_dummy(void *mydata) 
{
	return 0;
}

static void __phy_set_time_dummy(void *mydata, u_int64_t time) 
{
}

static void __phy_schedule_work_asap_dummy(void *mydata) 
{
}

static struct sk_buff* __phy_alloc_skb_dummy(void *mydata, int datalen) 
{
	return 0;
}

static void __phy_free_skb_dummy(void *mydata, struct sk_buff* skb)
{
	/*
	* Free the packet if it's not null -- not technically "nothing" but
	* may prevent some memory leakage in corner cases.     */
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
}

static int __phy_sendpacket_dummy(void *mydata, int max_packets_inflight, struct sk_buff* skb) 
{
	/** Free the packet if it's not null -- not technically "nothing" but
	* may prevent some memory leakage in corner cases.     */
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return VMAC_PHY_SENDPACKET_OK;
}

static int __phy_sendpacket_keepskbonfail_dummy(void *mydata, int max_packets_inflight, struct sk_buff* skb) 
{
	/** Free the packet if it's not null -- not technically "nothing" but
	* may prevent some memory leakage in corner cases.     */
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return VMAC_PHY_SENDPACKET_OK;
}

static u_int32_t __phy_get_duration_dummy(void *mydata,struct sk_buff* skb) 
{ 
	return 0; 
}

static u_int32_t __phy_get_txlatency_dummy(void *mydata) 
{
	return 0;
}

static int __mac_work_dummy(void *mydata, int intop)
{
	return 0;
}

static int __mac_packet_rx_dummy(void *mydata, struct sk_buff* skb, int intop)
{
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return VMAC_MAC_NOTIFY_OK;
}

static int __mac_packet_tx_done_dummy(void *mydata, struct sk_buff* skb, int intop)
{
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return VMAC_MAC_NOTIFY_OK;
}

static int __mac_packet_tx_dummy(void *mydata, struct sk_buff* skb, int intop)
{
	if (skb)
	{
		dev_kfree_skb_any(skb);
	}
	return VMAC_MAC_NOTIFY_OK;
}

static int __mac_attach_dummy(void *mydata, vmac_phy_t* phyinfo)
{
	return -1;
}

static int __mac_detach_dummy(void *mydata)
{
	return 0;
}

static int __mac_set_rx_func_dummy(void *mydata, mac_rx_func_t rxfunc, void *rxpriv)
{
	return 0;
}

static int __mac_set_unload_notify_func_dummy(void *mydata,mac_unload_notify_func_t unloadfunc, void *unloadpriv)
{
	return 0;
}

/* returns vmac_mac_t or vmac_phy_t cast to void **/
void *vmac_layer_new_instance(const char *name)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	head = __layer_hash(name);
	MYLIST_FOR_EACH(p, head)
	{
		vmac_layer_t *l = MYLIST_ENTRY(p, vmac_layer_t, name_hlist);
		if (!strncmp(l->name, name, VMAC_NAME_SIZE))
			return (l->new_instance)(l->layer_private);
	}
	return 0;
}

void vmac_layer_free_instance(void *inst)
{
	vmac_layer_t *l;

	/* XXX ugly! */
	l = ((vmac_mac_t *)(inst))->layer;
	l->destroy_instance(l->layer_private, inst);
}


void vmac_layer_register(vmac_layer_t *info)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	/* get list head with hash same as this name. there may be multiple nodes(layers) in this list  */
	head = __layer_hash(info->name);
	MYLIST_FOR_EACH(p, head)
	{/* iterate every node in this list */
		vmac_layer_t *l = MYLIST_ENTRY(p, vmac_layer_t, name_hlist);
		if (!strncmp(l->name, info->name, VMAC_NAME_SIZE))
		{
			printk("%s error: layer %s already registered\n", __func__, info->name);
			return;
		}
	}

	info->proc = __procfs_layer_add(info);
	/* add this info into list */
	MYLIST_ADD_HEAD(&info->name_hlist, head);

	printk("%s registered layer %s\n", __func__, info->name);
}

void vmac_layer_unregister(vmac_layer_t *info)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	head = __layer_hash(info->name);
	MYLIST_FOR_EACH(p, head)
	{
		vmac_layer_t *l = MYLIST_ENTRY(p, vmac_layer_t, name_hlist);
		if (l == info)
		{
			printk("%s unregistered layer %s\n", __func__, info->name);
			/* remove from procfs */
			__procfs_layer_del(info);
			info->proc = 0;
			/* remove from internal list */
			MYLIST_DEL(&l->name_hlist);
			return;
		}
	}
}

vmac_phy_t *vmac_alloc_phy(void)
{
	vmac_phy_t *phyinfo;

	phyinfo = kmalloc(sizeof(vmac_phy_t) ,GFP_ATOMIC);
	if (phyinfo)
	{
		vmac_init_phy(phyinfo);
		atomic_set (&phyinfo->refcnt, 1);
	}

	return phyinfo;
}

void vmac_free_phy(vmac_phy_t *phyinfo)
{
	if (phyinfo && atomic_dec_and_test(&phyinfo->refcnt))
	{
		printk("%s freed %s\n", __func__, phyinfo->name);
		if (phyinfo->phy_private)
			vmac_layer_free_instance(phyinfo);
		kfree(phyinfo);
	}
}

void vmac_init_phy(vmac_phy_t* phyinfo)
{
	memset(phyinfo, 0, sizeof(vmac_phy_t));
	
	strncpy(phyinfo->name, "dummy", VMAC_NAME_SIZE);
	
	phyinfo->attach_mac = __phy_attach_dummy;
	phyinfo->detach_mac = __phy_detach_dummy;
	phyinfo->phy_get_time = __phy_get_time_dummy;
	phyinfo->phy_set_time = __phy_set_time_dummy;
	phyinfo->phy_schedule_work_asap = __phy_schedule_work_asap_dummy;
	phyinfo->phy_alloc_skb = __phy_alloc_skb_dummy;
	phyinfo->phy_free_skb = __phy_free_skb_dummy;
	phyinfo->phy_sendpacket = __phy_sendpacket_dummy;
	phyinfo->phy_sendpacket_keepskbonfail= __phy_sendpacket_keepskbonfail_dummy;
	phyinfo->phy_get_duration = __phy_get_duration_dummy;
	phyinfo->phy_get_txlatency = __phy_get_txlatency_dummy;
}

void vmac_register_phy(vmac_phy_t* phyinfo)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	/* check for existing */
	head = __phyinfo_hash(phyinfo->name);
	MYLIST_FOR_EACH(p, head)
	{
		vmac_phy_t *m = MYLIST_ENTRY(p, vmac_phy_t, name_hlist);
		if (!strncmp(m->name, phyinfo->name, VMAC_NAME_SIZE))
		{
			printk("%s error: phy %s already registered\n", __func__, m->name);
			return;
		}
	}

	/* increment reference count */
	phyinfo = vmac_get_phy(phyinfo);
	/* add to /proc/softmac/insts */
	phyinfo->proc = __procfs_inst_add(phyinfo);
	/* add to the phyinfo list */
	MYLIST_ADD_HEAD(&phyinfo->name_hlist, head);

	printk("%s registered %s\n", __func__, phyinfo->name);
}

void vmac_unregister_phy(vmac_phy_t* phyinfo)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	head = __phyinfo_hash(phyinfo->name);
	MYLIST_FOR_EACH(p, head)
	{
		vmac_phy_t *p = MYLIST_ENTRY(p, vmac_phy_t, name_hlist);
		if (p == phyinfo)
		{
			vmac_mac_t *tmp = kmalloc(sizeof(vmac_mac_t), GFP_ATOMIC);
			strncpy(tmp->name, p->name, VMAC_NAME_SIZE);

			/* remove from internal pyinfo list */
			MYLIST_DEL(&p->name_hlist);
			/* decrement reference count */
			vmac_free_phy(p);
			/* remove from proc */
			__procfs_inst_del(tmp);
			kfree(tmp);
			printk("%s unregistered %s\n", __func__, phyinfo->name);
			return;
		}
	}
}

vmac_phy_t *vmac_get_phy(vmac_phy_t* phyinfo)
{
	if (phyinfo)
		atomic_inc(&phyinfo->refcnt);
	return phyinfo;
}

vmac_phy_t *vmac_get_phy_by_name(const char *name)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;
	vmac_phy_t *ret = 0;

	head = __phyinfo_hash(name);
	MYLIST_FOR_EACH(p, head)
	{
		vmac_phy_t *m = MYLIST_ENTRY(p, vmac_phy_t, name_hlist);
		if (!strncmp(m->name, name, VMAC_NAME_SIZE))
			ret = m;
	}

	if (ret)
		atomic_inc(&ret->refcnt);

	return ret;
}

vmac_mac_t *vmac_alloc_mac(void)
{
	vmac_mac_t *macinfo;
	macinfo = kmalloc(sizeof(vmac_mac_t),GFP_ATOMIC);
	if (macinfo)
	{
		vmac_init_mac(macinfo);
		atomic_set(&macinfo->refcnt, 1);
	}
	return macinfo;
}

void vmac_free_mac(vmac_mac_t *macinfo)
{

	if (macinfo && atomic_dec_and_test(&macinfo->refcnt))
	{
		printk("%s freed %s\n", __func__, macinfo->name);
		if (macinfo->mac_private)
			vmac_layer_free_instance(macinfo);
		kfree(macinfo);
	}
}

void vmac_init_mac(vmac_mac_t* macinfo)
{
	memset(macinfo, 0, sizeof(vmac_mac_t));
	macinfo->mac_tx = __mac_packet_tx_dummy;
	macinfo->mac_tx_done = __mac_packet_tx_done_dummy;
	macinfo->rx_packet = __mac_packet_rx_dummy;
	macinfo->mac_work = __mac_work_dummy;
	macinfo->attach_phy = __mac_attach_dummy;
	macinfo->detach_phy = __mac_detach_dummy;
	macinfo->set_rx_func = __mac_set_rx_func_dummy;
	macinfo->set_unload_notify_func = __mac_set_unload_notify_func_dummy;
}

void vmac_register_mac(vmac_mac_t* macinfo)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	head = __macinfo_hash(macinfo->name);
	MYLIST_FOR_EACH(p, head)
	{
		vmac_mac_t *m = MYLIST_ENTRY(p, vmac_mac_t, name_hlist);
		if (!strncmp(m->name, macinfo->name, VMAC_NAME_SIZE))
		{
			printk("%s error: mac %s already registered\n", __func__, m->name);
			return;
		}
	}

	/* increment reference count */
	macinfo = vmac_get_mac(macinfo);
	/* add to /proc/softmac/insts */
	macinfo->proc = __procfs_inst_add(macinfo);
	/* add to the macinfo list */
	MYLIST_ADD_HEAD(&macinfo->name_hlist, head);

	printk("%s registered %s\n", __func__, macinfo->name);
}

void vmac_unregister_mac(vmac_mac_t* macinfo)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;

	head = __macinfo_hash(macinfo->name);
	MYLIST_FOR_EACH(p, head)
	{
		vmac_mac_t *m = MYLIST_ENTRY(p, vmac_mac_t, name_hlist);
		if (m == macinfo)
		{
			/* save for removal from /proc/softmac/insts 
			* the directory cannot be removed before vmac_free_mac */
			vmac_mac_t *tmp = kmalloc(sizeof(vmac_mac_t), GFP_ATOMIC);
			strncpy(tmp->name, m->name, VMAC_NAME_SIZE);

			/* remove from internal list */
			MYLIST_DEL(&m->name_hlist);
			/* decrement reference count */
			vmac_free_mac(m);

			/* remove from proc */
			__procfs_inst_del(tmp);
			kfree(tmp);

			printk("%s unregistered %s\n", __func__, macinfo->name);
			return;
		}
	}
}

vmac_mac_t *vmac_get_mac(vmac_mac_t *macinfo)
{
	if (macinfo)
		atomic_inc(&macinfo->refcnt);
	return macinfo;
}

vmac_mac_t *vmac_get_mac_by_name(const char *name)
{
	MYLIST_HEAD *head;
	MYLIST_NODE *p;
	vmac_mac_t *ret = 0;

	head = __macinfo_hash(name);
	MYLIST_FOR_EACH(p, head)
	{
		vmac_mac_t *m = MYLIST_ENTRY(p, vmac_mac_t, name_hlist);
		if (!strncmp(m->name, name, VMAC_NAME_SIZE))
			ret = m;
	}

	if (ret)
		atomic_inc(&ret->refcnt);

	return ret;
}

static int __init softmac_core_init(void)
{
	__lists_init();
	__procfs_create();

	return 0;
}

static void __exit softmac_core_exit(void)
{
	trace;
	//softmac_lists_cleanup();
	__procfs_destroy();
}

module_init(softmac_core_init);
module_exit(softmac_core_exit);

EXPORT_SYMBOL(vmac_layer_register);
EXPORT_SYMBOL(vmac_layer_unregister);
EXPORT_SYMBOL(vmac_layer_new_instance);
EXPORT_SYMBOL(vmac_layer_free_instance );

EXPORT_SYMBOL(vmac_register_phy);
EXPORT_SYMBOL(vmac_unregister_phy);
EXPORT_SYMBOL(vmac_get_phy);
EXPORT_SYMBOL(vmac_get_phy_by_name);
EXPORT_SYMBOL(vmac_alloc_phy);
EXPORT_SYMBOL(vmac_free_phy);
EXPORT_SYMBOL(vmac_init_phy);

EXPORT_SYMBOL(vmac_register_mac);
EXPORT_SYMBOL(vmac_unregister_mac);
EXPORT_SYMBOL(vmac_get_mac);
EXPORT_SYMBOL(vmac_get_mac_by_name);
EXPORT_SYMBOL(vmac_alloc_mac);
EXPORT_SYMBOL(vmac_free_mac);
EXPORT_SYMBOL(vmac_init_mac);


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

/*
 * This is the rsmac example module
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
#include <linux/crc32.h>
#include "cu_softmac_api.h"
#include "softmac_rs.h"

MODULE_LICENSE("GPL");

static int rsmac_next_id;
static const char *rsmac_name = "rsmac";

//
// This is used as the magic identifier for a "reedsolomon" packet
//
static const char deadbeef = 0xf;

/**
 * Every VMAC MAC or PHY layer provides a vmac_layer_t interface
 */
static vmac_layer_t the_rsmac;

struct rsmac_inst_proc_data {
    struct list_head list;
    struct rsmac_instance *inst;
    int id;
    char name[VMAC_NAME_SIZE];
    struct proc_dir_entry *parent;
};

enum {
    RSMAC_INST_PROC_RESET_COUNTS,
    RSMAC_INST_PROC_RS_ENABLE,
    RSMAC_INST_PROC_RX_COUNT,
    RSMAC_INST_PROC_RX_DROP_COUNT,
    RSMAC_INST_PROC_FIXED_ERROR_COUNT,
    RSMAC_INST_PROC_FAKE_ERROR_RATE,
};

struct rsmac_inst_proc_entry {
    const char *name;
    mode_t mode;
    int id;
};

static const struct rsmac_inst_proc_entry rsmac_inst_proc_entries[] = {
    {
	"reset_counts",
	0200,
	RSMAC_INST_PROC_RESET_COUNTS
    },
    {
	"rx_count",
	0444,
	RSMAC_INST_PROC_RX_COUNT
    },
    {
	"rs_enable",
	0644,
	RSMAC_INST_PROC_RS_ENABLE
    },
    {
	"rx_drop_count",
	0444,
	RSMAC_INST_PROC_RX_DROP_COUNT
    },
    {
	"fixed_error_count",
	0444,
	RSMAC_INST_PROC_FIXED_ERROR_COUNT
    },
    {
	"fake_error_rate",
	0644,
	RSMAC_INST_PROC_FAKE_ERROR_RATE
    },
    /* terminator, don't remove */
    {
	0,
	0,
	-1
    },
};

/* private instance data */
struct rsmac_instance {
    vmac_mac_t *macinfo;
    vmac_phy_t *phyinfo;

    mac_rx_func_t netif_rx;
    void *netif_rx_priv;

    struct tasklet_struct rxq_tasklet;
    struct sk_buff_head   rxq;

    int fake_error_rate;
    int fixed_error_count;
    int rx_drop_count;
    int rx_count;
    int rs_enable;

    int id;
    struct proc_dir_entry *procfs_dir;
    struct list_head procfs_data;
    rwlock_t lock;
};

/*
//
// Yes, this is a bogus CRC. We'll fix later
//
int boguscrc(char *p, int l)
{
  int a = 0;
  int i;
  for (i = 0; i < l; i++) {
    a += p[i];
  }
  return a;
}
*/

static int
rsmac_inst_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data) 
{
    int result = 0;
    struct rsmac_inst_proc_data* procdata = data;
    if (procdata && procdata->inst) {
	struct rsmac_instance* inst = procdata->inst;
	char* dest = (page + off);
	int intval = 0;
	
	switch (procdata->id) {
	case RSMAC_INST_PROC_RX_COUNT:
	    result = snprintf(dest, count, "%d\n", inst->rx_count);
	    break;
	case RSMAC_INST_PROC_RX_DROP_COUNT:
	    result = snprintf(dest, count, "%d\n", inst->rx_drop_count);
	    break;
	case RSMAC_INST_PROC_FIXED_ERROR_COUNT:
	    result = snprintf(dest, count, "%d\n", inst->fixed_error_count);
	    break;
	case RSMAC_INST_PROC_FAKE_ERROR_RATE:
	    result = snprintf(dest, count, "%d\n", inst->fake_error_rate);
	    break;
	case RSMAC_INST_PROC_RS_ENABLE:
	    result = snprintf(dest, count, "%d\n", inst->rs_enable);
	    break;
	default:
	    result = 0;
	    *eof = 1;
	    break;
	}
    }
    return result;
}

static int
rsmac_inst_write_proc(struct file *file, const char __user *buffer, 
		       unsigned long count, void *data)
{
    int result = 0;
    struct rsmac_inst_proc_data* procdata = data;
    unsigned long flags;

    if (procdata && procdata->inst) {
	struct rsmac_instance* inst = procdata->inst;
	static const int maxkdatalen = 256;
	char kdata[maxkdatalen];
	char* endp = 0;
	long intval = 0;
	
	/*
	 * Drag the data over into kernel land
	 */
	if (maxkdatalen <= count) {
	    copy_from_user(kdata,buffer,(maxkdatalen-1));
	    kdata[maxkdatalen-1] = 0;
	    result = (maxkdatalen-1);
	}
	else {
	    copy_from_user(kdata,buffer,count);
	    result = count;
	}
	
	switch (procdata->id) {
	case RSMAC_INST_PROC_RESET_COUNTS:
	    intval = simple_strtol(kdata, &endp, 10);
	    write_lock_irqsave(&(inst->lock), flags);
	    inst->rx_count = 0;
	    inst->rx_drop_count = 0;
	    inst->fixed_error_count = 0;
	    write_unlock_irqrestore(&(inst->lock), flags);
	    break;
	case RSMAC_INST_PROC_FAKE_ERROR_RATE:
	    intval = simple_strtol(kdata, &endp, 10);
	    write_lock_irqsave(&(inst->lock), flags);
	    inst->fake_error_rate = intval;
	    write_unlock_irqrestore(&(inst->lock), flags);
	    break;
	case RSMAC_INST_PROC_RS_ENABLE:
	    intval = simple_strtol(kdata, &endp, 10);
	    write_lock_irqsave(&(inst->lock), flags);
	    inst->rs_enable = (intval == 0) ? 0 : 1;
	    write_unlock_irqrestore(&(inst->lock), flags);
	    break;
	default:
	    break;
	}
    }
    else {
	result = count;
    }
    
    return result;
}

static int
rsmac_make_procfs_entries(struct rsmac_instance *inst)
{
    int i, result = 0;
    struct proc_dir_entry *ent;
    struct rsmac_inst_proc_data *proc_data;

    if (inst) {
	inst->procfs_dir = inst->macinfo->proc;
	
	for (i=0; (rsmac_inst_proc_entries[i].name && rsmac_inst_proc_entries[i].name[0]); i++) {
	    ent = create_proc_entry(rsmac_inst_proc_entries[i].name,
				    rsmac_inst_proc_entries[i].mode,
				    inst->procfs_dir);
	    ent->owner = THIS_MODULE;
	    ent->read_proc = rsmac_inst_read_proc;
	    ent->write_proc = rsmac_inst_write_proc;
	    ent->data = kmalloc(sizeof(struct rsmac_inst_proc_data), GFP_ATOMIC);

	    proc_data = ent->data;
	    INIT_LIST_HEAD(&(proc_data->list));
	    list_add_tail(&(proc_data->list), &(inst->procfs_data));
	    proc_data->inst = inst;
	    proc_data->id = rsmac_inst_proc_entries[i].id;
	    strncpy(proc_data->name, rsmac_inst_proc_entries[i].name, VMAC_NAME_SIZE);
	    proc_data->parent = inst->procfs_dir;
	}
    }
    return result;
}

static int
rsmac_delete_procfs_entries(struct rsmac_instance *inst)
{
    int result = 0;
    if (inst) {
	struct list_head* tmp = 0;
	struct list_head* p = 0;
	struct rsmac_inst_proc_data* proc_entry_data = 0;
	
	/*
	 * Remove individual entries and delete their data
	 */
	list_for_each_safe(p, tmp, &(inst->procfs_data)) {
	    proc_entry_data = list_entry(p, struct rsmac_inst_proc_data, list);
	    list_del(p);
	    remove_proc_entry(proc_entry_data->name, proc_entry_data->parent);
	    kfree(proc_entry_data);
	    proc_entry_data = 0;
	}
	
    }
    return result;
}

/* attach to a phy layer */
static int
rsmac_mac_attach(void *me, vmac_phy_t *phyinfo)
{
    //printk("%s\n", __func__);
    struct rsmac_instance *inst = me;
    int ret = -1;
    unsigned long flags;

    write_lock_irqsave(&(inst->lock), flags);
    if (!inst->phyinfo) {
	inst->phyinfo = vmac_get_phy(phyinfo);
	ret = 0;
    }
    write_unlock_irqrestore(&(inst->lock), flags);
    return ret;
}

/* detach from phy layer */
static int
rsmac_mac_detach(void *me)
{
    //printk("%s\n", __func__);
    struct rsmac_instance *inst = me;
    unsigned long flags;

    write_lock_irqsave(&(inst->lock), flags);
    if (inst->phyinfo) {
	vmac_free_phy(inst->phyinfo);
	inst->phyinfo = 0;
    }
    write_unlock_irqrestore(&(inst->lock), flags);
    return 0;
}

static int
rsmac_mac_set_netif_rx_func(void *me, 
			    mac_rx_func_t rxfunc, 
			    void *rxpriv) 
{
    //printk("%s\n", __func__);
    struct rsmac_instance *inst = me;
    unsigned long flags;

    write_lock_irqsave(&(inst->lock), flags);

    inst->netif_rx = rxfunc;
    inst->netif_rx_priv = rxpriv;

    write_unlock_irqrestore(&(inst->lock), flags);

    return 0;
}

static int
rsmac_mac_packet_tx(void *me, struct sk_buff *skb, int intop)
{
    //printk("%s\n", __func__);
    struct rsmac_instance *inst = me; 

    BUG_ON(intop);

    read_lock(&(inst->lock));
    if (inst->phyinfo) {
      //XXX Remove int crc = boguscrc(skb->data, skb->len);
      u32 crc = crc32_le(0,skb->data, skb->len);
      //
      // Put in CRC
      //
      skb = skb_padto(skb, skb -> len + sizeof(crc));
      char *crcptr = skb_put(skb, sizeof(crc));
      //XXX Remove *((int*) crcptr) = crc;
      *((u32*) crcptr) = crc;

      // RS encode (including the CRC)
      if (inst->rs_enable) {
	skb = cu_softmac_rs_encode_skb(skb, 0);
      }
      //
      // Prepend packet header
      //
      skb_cow(skb, sizeof(deadbeef));
      char *label = skb_push(skb, sizeof(deadbeef)); 
      *label = deadbeef;

      if (inst->fake_error_rate) {
	cu_softmac_rs_make_errors(skb, 0, inst->fake_error_rate);
      }
      inst->phyinfo->phy_sendpacket(inst->phyinfo->phy_private, 1, skb);
    }
    else { 
	kfree_skb(skb);
    }
    read_unlock(&(inst->lock));

    return VMAC_MAC_NOTIFY_OK;
}

static void
rsmac_rx_tasklet(unsigned long data)
{
    //printk("%s\n", __func__);
    struct rsmac_instance *inst = (struct rsmac_instance *)data;
    struct sk_buff *skb;
    
    read_lock(&(inst->lock));

    while ( (skb = skb_dequeue(&(inst->rxq))) ) {
	inst->rx_count++;
	if (inst->netif_rx) {
	  inst->netif_rx(inst->netif_rx_priv, skb);
	  continue;
	} 
	// drop
	inst->rx_drop_count++;
	if (inst->phyinfo)
	    inst->phyinfo->phy_free_skb(inst->phyinfo->phy_private, skb);
	else
	    kfree_skb(skb);
    }

    read_unlock(&(inst->lock));
}

static int
rsmac_mac_packet_rx(void *me, struct sk_buff *skb, int intop)
{
    //printk("%s\n", __func__);

    struct rsmac_instance *inst = me;


    char *src = skb->data;
    if ( skb -> len > 1 && *src == deadbeef ) {
      //
      // Yes, it's ours. Decode it
      //
      skb_pull(skb, sizeof(deadbeef));

      if (inst->rs_enable) {
	int errcnt = 0;
	skb = cu_softmac_rs_decode_skb(skb, &errcnt);
	inst->fixed_error_count += errcnt;
      }
      //
      // Compute the checksum
      //XXX Remove int crc = boguscrc(skb->data, skb->len-4);
      u32 crc = crc32_le(0,skb->data, skb->len-4);
      char *crcptr = skb -> data + skb->len-4;      
      //XXX Remove int msgcrc = *(int *)crcptr;
      u32 msgcrc = *(u32 *)crcptr;
      skb_trim(skb, skb->len-4);
      if (crc == msgcrc) {
	skb_queue_tail(&(inst->rxq), skb);
	if (intop) {
	  tasklet_schedule(&(inst->rxq_tasklet));
	} else {
	  rsmac_rx_tasklet((unsigned long)me);
	}
	return 0;
      } else {
	// It's ours, but checksum is bad
	return 1;
      }
    } else {
      //
      // Tell multimac it is not our packet
      //
      return -1;
    }
} 



/* create and return a new rsmac instance */
static void *
rsmac_new_instance (void *layer_priv)
{
    //printk("%s\n", __func__);

    struct rsmac_instance *inst;
    void *ret = 0;

    inst = kmalloc(sizeof(struct rsmac_instance), GFP_ATOMIC);
    if (inst) {
	memset(inst, 0, sizeof(struct rsmac_instance));

	inst->lock = RW_LOCK_UNLOCKED;
	inst->id = rsmac_next_id++;
	inst->rs_enable = 1;

	/* setup the macinfo structure */
	inst->macinfo = vmac_alloc_mac();
	inst->macinfo->mac_private = inst;
	inst->macinfo->layer = &the_rsmac;
	snprintf(inst->macinfo->name, VMAC_NAME_SIZE, "%s%d", the_rsmac.name, inst->id);

	/* override some macinfo functions */
	/* the rest remain pointed at the default "do nothing" functions */
	inst->macinfo->mac_tx = rsmac_mac_packet_tx;
	inst->macinfo->rx_packet = rsmac_mac_packet_rx;
	inst->macinfo->attach_phy = rsmac_mac_attach;
	inst->macinfo->detach_phy = rsmac_mac_detach;
	inst->macinfo->set_rx_func = rsmac_mac_set_netif_rx_func;

	/* setup rx tasklet */
	tasklet_init(&(inst->rxq_tasklet), rsmac_rx_tasklet, (unsigned long)inst);
	skb_queue_head_init(&(inst->rxq));
	
	/* register with softmac */
	vmac_register_mac(inst->macinfo);
	
	/* setup procfs */
	inst->procfs_dir = (struct proc_dir_entry *)inst->macinfo->proc;
	INIT_LIST_HEAD(&inst->procfs_data);
	rsmac_make_procfs_entries(inst);

	/* we've registered with softmac, decrement the ref count */
	vmac_free_mac(inst->macinfo);

	ret = inst->macinfo;
    }
    return ret;
}

/* called by softmac_core when a rsmac vmac_mac_t
 * instance is deallocated 
 */
static void
rsmac_free_instance (void *layer_priv, void *info)
{
    //printk("%s\n", __func__);
    vmac_mac_t *macinfo = info;
    struct rsmac_instance *inst = macinfo->mac_private;
    if (inst->phyinfo) {
	vmac_free_phy(inst->phyinfo);
    }
    rsmac_delete_procfs_entries(inst);
    kfree(inst);
}

static int __init 
softmac_rsmac_init(void)
{
    //printk("%s\n", __func__);
    /* register the rsmac layer with softmac */
    strncpy(the_rsmac.name, rsmac_name, VMAC_NAME_SIZE);
    the_rsmac.new_instance = rsmac_new_instance;
    the_rsmac.destroy_instance = rsmac_free_instance;
    vmac_layer_register(&the_rsmac);

    return 0;
}

static void __exit 
softmac_rsmac_exit(void)
{
    //printk("%s\n", __func__);
    /* tell softmac we're leaving */
    vmac_layer_unregister((vmac_layer_t *)&the_rsmac);
}

module_init(softmac_rsmac_init);
module_exit(softmac_rsmac_exit);

/**/

/*
 * This is the NullMAC example module
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/list.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include "cu_softmac_api.h"

MODULE_LICENSE("GPL");

/* private instance data */
typedef struct {
    vmac_mac_t *macinfo;
    vmac_phy_t *phyinfo;

    mac_rx_func_t myrxfunc;
    void *myrxfunc_priv;

    int id;
} NULLMAC_INSTANCE;

static int nullmac_next_id;

/**
 * Every VMAC MAC or PHY layer provides a vmac_layer_t interface
 */
static vmac_layer_t the_nullmac;

/* attach to a phy layer */
static int
nullmac_mac_attach(void *me, vmac_phy_t *phyinfo)
{
    NULLMAC_INSTANCE *inst = me;
    if (!inst->phyinfo) {
	inst->phyinfo = vmac_get_phy(phyinfo);
	return 0;
    }
    /* error: already attached */
    return -1;
}

/* detach from phy layer */
static int
nullmac_mac_detach(void *me)
{
    NULLMAC_INSTANCE *inst = me;

    if (inst->phyinfo) {
	vmac_free_phy(inst->phyinfo);
	inst->phyinfo = 0;
    }
    return 0;
}

static int
nullmac_mac_set_rx_func(void *me, mac_rx_func_t rxfunc, void* rxpriv)
{
    NULLMAC_INSTANCE *inst = me;
    if (inst && rxfunc) {
	inst->myrxfunc = rxfunc;
	inst->myrxfunc_priv = rxpriv;
    } else {
	printk("%s error\n", __func__);
    }
}

static int my_rxhelper(void* mydata, struct sk_buff* packet, int intop)  
{
    NULLMAC_INSTANCE *inst = mydata;
    printk("rx func %s\n", the_nullmac.name);
    if (inst && inst->myrxfunc)
	(inst->myrxfunc)(inst->myrxfunc_priv, packet);
    else
	printk("%s error\n", __func__);
}			 
			 
static int my_txhelper(void* mydata, struct sk_buff* packet, int intop) {
	printk("tx func %s\n", the_nullmac.name);
	multimac_tx(mydata, packet, 0);
}


/* create and return a new nullmac instance */
static void *
nullmac_new_instance (void *layer_priv)
{
    printk("%s\n", __func__);

    NULLMAC_INSTANCE *inst;
    void *ret = 0;

    inst = kmalloc(sizeof(NULLMAC_INSTANCE), GFP_ATOMIC);
    if (inst) {
	memset(inst, 0, sizeof(NULLMAC_INSTANCE));

	inst->id = nullmac_next_id++;

	/* setup the macinfo structure */
	inst->macinfo = vmac_alloc_mac();
	inst->macinfo->mac_private = inst;
	inst->macinfo->layer = &the_nullmac;
	inst->macinfo->rx_packet = my_rxhelper;
	inst->macinfo->mac_tx = my_txhelper;
	inst->macinfo->set_rx_func = nullmac_mac_set_rx_func;

	snprintf(inst->macinfo->name, VMAC_NAME_SIZE, "%s%d", the_nullmac.name, inst->id);

	/* override some macinfo functions */
	/* the rest remain pointed at the default "do nothing" functions */
	inst->macinfo->attach_phy = nullmac_mac_attach;
	inst->macinfo->detach_phy = nullmac_mac_detach;

	/* register with softmac */
	vmac_register_mac(inst->macinfo);

	/* note: an instance shouldn't hold a reference to it's own macinfo because
	 * they are really just public and private parts of the same object.
	 */
	/* we've registered with softmac, decrement the ref count */
	vmac_free_mac(inst->macinfo);

	ret = inst->macinfo;
    }
    return ret;
}

/* called by softmac_core when a nullmac vmac_mac_t
 * instance is deallocated 
 */
static void
nullmac_free_instance (void *layer_priv, void *info)
{
    printk("%s\n", __func__);

    vmac_mac_t *macinfo = info;
    NULLMAC_INSTANCE *inst = macinfo->mac_private;
    if (inst->phyinfo) {
	vmac_free_phy(inst->phyinfo);
    }
    kfree(inst);
}

static int __init 
softmac_nullmac_init(void)
{
    printk("%s\n", __func__);

    /* register the nullmac layer with softmac */
    strncpy(the_nullmac.name, "mac1", VMAC_NAME_SIZE);
    the_nullmac.new_instance = nullmac_new_instance;
    the_nullmac.destroy_instance = nullmac_free_instance;
    vmac_layer_register(&the_nullmac);

    return 0;
}

static void __exit 
softmac_nullmac_exit(void)
{
    printk("%s\n", __func__);

    /* tell softmac we're leaving */
    vmac_layer_unregister((vmac_layer_t *)&the_nullmac);
}

module_init(softmac_nullmac_init);
module_exit(softmac_nullmac_exit);

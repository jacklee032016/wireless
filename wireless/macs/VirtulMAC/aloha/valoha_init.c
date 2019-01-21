
#include "valoha.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Michael Neufeld");


static LIST_HEAD( alohaInstsList);

static vmac_layer_t alohaMac;

/**
 *  Some operations, e.g. getting/setting the next instance ID
 * and accessing parameters, should be performed "atomically."
 */
static spinlock_t alohaLock = SPIN_LOCK_UNLOCKED;

static int alohaNextId = 1;

static int alohaTxBitrate = VALOHA_DEFAULT_TXBITRATE;

/**
 *  Whether or not to defer handling of packet transmission.
 */
static int alohaDeferTx = VALOHA_DEFAULT_DEFERTX;
static int alohaDeferTxDone = VALOHA_DEFAULT_DEFERTXDONE;
static int alohaDeferRx = VALOHA_DEFAULT_DEFERRX;
static int alohaMaxInflight = VALOHA_DEFAULT_MAXINFLIGHT;

#if 0
/*
 * XXX
 * use the ath-specific "deferallrx" and "deferalltxdone"?
 */
static int cheesymac_ath_deferallrx = VALOHA_DEFAULT_DEFERALLRX;
static int cheesymac_ath_deferalltxdone = VALOHA_DEFAULT_DEFERALLTXDONE;
#endif

/*
 * Default network prefix to use for the OS-exported network interface
 * attached to the MAC layer. The unique cheesyMAC ID is used
 * to fill out the "%d" field
 */
static char* alohaNetifName = VMAC_MODULE_ALOHA"%d";

#if __ARM__
MODULE_PARM( alohaDeferTx, "1i");
MODULE_PARM( alohaDeferTxDone, "1i");
MODULE_PARM( alohaDeferRx, "1i");
MODULE_PARM( alohaMaxInflight, "1i");
MODULE_PARM( alohaTxBitrate, "1i");
#else
module_param(alohaDeferTx, int, 0644);
module_param(alohaDeferTxDone, int, 0644);
module_param(alohaDeferRx, int, 0644);
module_param(alohaMaxInflight, int, 0644);
module_param(alohaTxBitrate, int, 0644);
#endif

MODULE_PARM_DESC(alohaDeferTx, "Queue packets and defer transmit to tasklet");
MODULE_PARM_DESC(alohaDeferTxDone, "Queue packets that are finished transmitting and defer handling to tasklet");
MODULE_PARM_DESC(alohaDeferRx, "Queue received packets and defer handling to tasklet");
MODULE_PARM_DESC(alohaMaxInflight, "Limit the number of packets allowed to be in the pipeline for transmission");
MODULE_PARM_DESC(alohaTxBitrate, "Default bitrate to use when transmitting packets");



/* Get the default parameters used to initialize new CheesyMAC instances */
void valoha_get_default_params(aloha_param_t* params)
{
	if (params)
	{
		spin_lock( &alohaLock);
		params->txbitrate = alohaTxBitrate;
		params->defertx = alohaDeferTx;
		params->defertxdone = alohaDeferTxDone;
		params->deferrx = alohaDeferRx;
		params->maxinflight = alohaMaxInflight;
		spin_unlock( &alohaLock);
	}
	else
	{
		printk(KERN_DEBUG VMAC_MODULE_ALOHA": Called get_default_params with null parameters!\n");
	}
}

/* Set the default parameters used to initialize new CheesyMAC instances */
void valoha_set_default_params(aloha_param_t* params)
{
	if (params)
	{
		spin_lock(&alohaLock);
		alohaTxBitrate = params->txbitrate;
		alohaDeferTx = params->defertx;
		alohaDeferTxDone = params->defertxdone;
		alohaDeferRx = params->deferrx;
		alohaMaxInflight = params->maxinflight;
		spin_unlock(&alohaLock);
	}
	else
	{
		printk(KERN_DEBUG VMAC_MODULE_ALOHA": Called set_default_params with null parameters!\n");
	}
}

/* Get the parameters of a specific CheesyMAC instance */
void valoha_get_instance_params(void* macpriv, aloha_param_t* params)
{
	if (macpriv && params)
	{
		valoha_inst_t* inst = macpriv;
		read_lock(&(inst->mac_busy));
		params->txbitrate = inst->txbitrate;
		params->defertx = inst->defertx;
		params->defertxdone = inst->defertxdone;
		params->deferrx = inst->deferrx;
		params->maxinflight = inst->maxinflight;
		read_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_DEBUG VMAC_MODULE_ALOHA": Called get_instance_params with bad data!\n");
	}
}

/* Set the parameters of a specific instance */
void valoha_set_instance_params(void* macpriv, aloha_param_t* params)
{
	if (macpriv && params)
	{
		valoha_inst_t* inst = macpriv;
		write_lock(&(inst->mac_busy));
		inst->txbitrate = params->txbitrate;
		inst->defertx = params->defertx;
		inst->defertxdone = params->defertxdone;
		inst->deferrx = params->deferrx;
		inst->maxinflight = params->maxinflight;
		write_unlock(&(inst->mac_busy));
	}
	else
	{
		printk(KERN_DEBUG VMAC_MODULE_ALOHA": Called set_instance_params with bad data!\n");
	}
}

static valoha_inst_t *__valoha_new_instance(vmac_mac_t* macinfo, aloha_param_t* params) 
{
	valoha_inst_t* inst;

	inst = kmalloc(sizeof(valoha_inst_t), GFP_ATOMIC);
	if (inst)
	{
		memset(inst, 0, sizeof(valoha_inst_t));

		inst->mac_busy = RW_LOCK_UNLOCKED;
		write_lock(&(inst->mac_busy));
		/* add new instance to list */
		INIT_LIST_HEAD(&inst->list);
		list_add_tail(&inst->list, &alohaInstsList);
		INIT_LIST_HEAD(&inst->my_procfs_data);

		/* fire up the instance... */
		inst->defaultphy = vmac_alloc_phy();
		inst->myphy = inst->defaultphy;
		inst->mymac = macinfo;

		/* access the global cheesymac variables safely */
		spin_lock( &alohaLock);
		inst->instanceid = alohaNextId;
		alohaNextId++;
		inst->txbitrate = alohaTxBitrate;
		inst->defertx = alohaDeferTx;
		inst->defertxdone = alohaDeferTxDone;
		inst->deferrx = alohaDeferRx;
		inst->maxinflight = alohaMaxInflight;
		spin_unlock( &alohaLock);

		/* setup params if any were passed in */
		if (params)
		{
			inst->txbitrate = params->txbitrate;
			inst->defertx = params->defertx;
			inst->defertxdone = params->defertxdone;
			inst->deferrx = params->deferrx;
			inst->maxinflight = params->maxinflight;
		}

		/* initialize our packet queues */
		skb_queue_head_init(&(inst->tx_skbqueue));
		skb_queue_head_init(&(inst->txdone_skbqueue));
		skb_queue_head_init(&(inst->rx_skbqueue));

		/* load up the function table so that other layers can communicate with us */
		valoha_set_mac_funcs(macinfo);

		/* finish macinfo setup */
		macinfo->layer = &alohaMac;
		macinfo->mac_private = inst;
		snprintf(macinfo->name, VMAC_NAME_SIZE, alohaNetifName, inst->instanceid);

		/* release write lock */
		write_unlock(&(inst->mac_busy));

		/* create and attach to our Linux network interface */
		valoha_netif(inst);
	}
	else
	{
		printk(KERN_ALERT VMAC_MODULE_ALOHA" : Unable to allocate memory when created!\n");
	}

	return inst;
}

/** Detach/delete this mac instance */
 void valoha_destroy_instance(void* mypriv) 
{
	valoha_inst_t* inst = mypriv;
	struct sk_buff* skb = 0;

	if (inst)
	{
		list_del(&(inst->list));
		valoha_detach(mypriv);

		/* Notify folks that we're going away... */
		if (inst->myunloadnotifyfunc)
		{
			(inst->myunloadnotifyfunc)(inst->myunloadnotifyfunc_priv);
		}

		write_lock(&(inst->mac_busy));

		valoha_delete_procfs(inst);

		/* Drain queues */
		while ((skb = skb_dequeue(&(inst->tx_skbqueue))))
		{
			(inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
		}

		while ((skb = skb_dequeue(&(inst->txdone_skbqueue))))
		{
			(inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
		}

		while ((skb = skb_dequeue(&(inst->rx_skbqueue))))
		{
			(inst->myphy->phy_free_skb)(inst->myphy->phy_private, skb);
		}

		inst->mymac = 0;
		inst->myphy = 0;
		vmac_free_phy(inst->defaultphy);
		inst->defaultphy = 0;

		write_unlock(&(inst->mac_busy));

		kfree(inst);
    }
}


void *valoha_vmac_new_instance(void *layer_priv)
{
	vmac_mac_t *macinfo;
	valoha_inst_t *inst;

	macinfo = vmac_alloc_mac();
	if (!macinfo)
	{
		return 0;
	}

	inst = __valoha_new_instance(macinfo, 0);
	if (!inst)
	{
		vmac_free_mac(macinfo);
		return 0;
	}

	write_lock(&(inst->mac_busy));

	/* setup macinfo and register it with softmac */
	vmac_register_mac(macinfo);

	/* create procfs entries -- must come after vmac_register_mac */
	inst->my_procfs_root = aloha_proc_handle;
	valoha_make_procfs(inst);

	/* instances don't keep references to themselves */
	vmac_free_mac(macinfo);

	write_unlock(&(inst->mac_busy));

	return macinfo;
}

void valoha_vmac_free_instance(void *layer_priv, void *info)
{
	vmac_mac_t *macinfo = info;
	valoha_inst_t *inst = macinfo->mac_private;
	valoha_destroy_instance(inst);
}
  

static int __init valoha_init(void)
{
	printk(KERN_DEBUG "Loading "VMAC_MODULE_ALOHA"\n");

	strncpy( alohaMac.name, VMAC_MODULE_ALOHA, VMAC_NAME_SIZE);
	alohaMac.new_instance = valoha_vmac_new_instance;
	alohaMac.destroy_instance = valoha_vmac_free_instance;

	vmac_layer_register(&alohaMac);
	aloha_proc_handle = alohaMac.proc;

	return 0;
}

static void __exit valoha_exit(void)
{
	printk(KERN_DEBUG "Unloading "VMAC_MODULE_ALOHA"\n");

	/* Tell any/all softmac PHY layers that we're leaving*/
	spin_lock( &alohaLock);
	if (!list_empty(&alohaInstsList))
	{
		printk(KERN_DEBUG VMAC_MODULE_ALOHA": Deleting instances\n");
		valoha_inst_t* inst = 0;
		struct list_head* tmp = 0;
		struct list_head* p = 0;

		/* Walk through all instances, remove from the linked 
		* list and then dispose of them cleanly.  */
		list_for_each_safe(p,tmp,&alohaInstsList)
		{
			inst = list_entry(p,valoha_inst_t, list);
			printk(KERN_DEBUG VMAC_MODULE_ALOHA": Detaching and destroying instance ID %d\n", inst->instanceid);
			valoha_destroy_instance( inst );
		}
	}
	else
	{
		printk(KERN_DEBUG VMAC_MODULE_ALOHA" No instances found\n");
	}

	vmac_layer_unregister(&alohaMac);

	spin_unlock( &alohaLock);
}

module_init(valoha_init);
module_exit(valoha_exit);

EXPORT_SYMBOL(valoha_vmac_new_instance);
EXPORT_SYMBOL(valoha_vmac_free_instance);
EXPORT_SYMBOL(valoha_set_mac_funcs);
EXPORT_SYMBOL(valoha_get_default_params);
EXPORT_SYMBOL(valoha_set_default_params);
EXPORT_SYMBOL(valoha_get_instance_params);
EXPORT_SYMBOL(valoha_set_instance_params);

#if 0
/*
 * XXX keeping these around as examples...
 */
static short int myshort = 1;
static int myint = 420;
static long int mylong = 9999;
static char *mystring = "blah";

/* 
 * module_param(foo, int, 0000)
 * The first param is the parameters name
 * The second param is it's data type
 * The final argument is the permissions bits, 
 * for exposing parameters in sysfs (if non-zero) at a later stage.
 */

module_param(myshort, short, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(myshort, "A short integer");
module_param(myint, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(myint, "An integer");
module_param(mylong, long, S_IRUSR);
MODULE_PARM_DESC(mylong, "A long integer");
module_param(mystring, charp, 0000);
MODULE_PARM_DESC(mystring, "A character string");
#endif


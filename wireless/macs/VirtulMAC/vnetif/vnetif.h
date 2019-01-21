/********/

#ifndef __VNETIF_H__
#define __VNETIF_H__

#define VMAC_MIN_MTU 			68
#define VMAC_MAX_MTU 			2290

typedef void* vnet_handle;

typedef int (*vnet_tx_func_t)(vnet_handle,void*,struct sk_buff* packet);
typedef int (*vnet_notify_func_t)(vnet_handle,void*);

/* This function creates an ethernet interface */
vnet_handle vmac_create_vnetif(char* name, unsigned char* macaddr, vnet_tx_func_t txfunc, void* txfunc_priv);

/* Destroy a previously created ethernet interface  */
void vmac_destroy_vnetif(vnet_handle nif);

/* Detach the current client  */
void vmac_vnetif_detach(vnet_handle nif);

/*
 * A client should call this function when it has a packet ready
 * to send up to higher layers of the network stack.
 * XXX this assumes ethernet-encapsulated packets -- should
 * make things more general at some point.
 */
enum
{
	VNETIF_RX_PACKET_OK 		= 0,
	VNETIF_RX_PACKET_BUSY 	= 1,
	VNETIF_RX_PACKET_ERROR 	= 2,
};

int vmac_vnetif_rx(vnet_handle nif,struct sk_buff* packet);


/** Set the function to call when a packet is ready for transmit */
void vmac_vnetif_set_tx_callback(vnet_handle nif, vnet_tx_func_t txfunc, void* txfunc_priv);

/* Set the function to call when the interface is being unloaded */
void vmac_netif_set_unload_callback(vnet_handle nif, vnet_notify_func_t unloadfunc, void* unloadfunc_priv);

/*
 * Get a netif handle from a dev. Dangerous if you aren't absolutely
 * sure that the device is of type netif... Should devise some way of checking for this. */
vnet_handle vmac_get_vnetif(struct net_device* netdev);

/** Get a net_device from netif handle  */
struct net_device *vmac_get_netdev(vnet_handle nif);

#endif

/*
 * softmac_maclayer.{cc,hh} -- interface between CU VMAC API and Click
 * Michael Neufeld
 * Jeff Fifield
 *
 * Copyright (c) 2005 University of Colorado at Boulder
 *
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose other than its incorporation into a
 *  commercial product is hereby granted without fee, provided that the
 *  above copyright notice appear in all copies and that both that
 *  copyright notice and this permission notice appear in supporting
 *  documentation, and that the name of the University not be used in
 *  advertising or publicity pertaining to distribution of the software
 *  without specific, written prior permission.
 *
 *  UNIVERSITY OF COLORADO DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 *  SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *  FITNESS FOR ANY PARTICULAR PURPOSE.  IN NO EVENT SHALL THE UNIVERSITY
 *  OF COLORADO BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 *  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 *  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 *  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 *  PERFORMANCE OF THIS SOFTWARE.
 *
 */
#include <click/config.h>
#include <click/error.hh>
#include <click/glue.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include "cu_softmac_api.h"
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>
#include "softmac_maclayer.hh"

SoftmacMACLayer::SoftmacMACLayer()
{
    click_chatter("SoftmacMACLayer: constructor\n");
    
    /* _defaultmac is a do nothing mac instance to avoid null checks on _macinfo */
    _defaultmac = vmac_alloc_mac();
    macinfo_init(this, _defaultmac);
    _macinfo = _defaultmac;
    _packetrxsink = &_defaultsink;
    _packettxdonesink = &_defaultsink;
}

SoftmacMACLayer::~SoftmacMACLayer()
{
    click_chatter("SoftmacMACLayer: destructor\n");
    vmac_free_mac(_defaultmac);
}

SoftmacMACLayer*
SoftmacMACLayer::make(vmac_mac_t *macinfo)
{
    SoftmacMACLayer *smac = 0;
    if (macinfo) {
	smac = new SoftmacMACLayer;
	if (smac)
	    smac->_macinfo = macinfo;
    }
    return smac;
}

SoftmacMACLayer* 
SoftmacMACLayer::make(const char *mac_instance_name)
{
    vmac_mac_t *m = vmac_get_mac_by_name(mac_instance_name);
    SoftmacMACLayer *smac = make(m);
    vmac_free_mac(m);
    return smac;
}

SoftmacMACLayer* 
SoftmacMACLayer::make()
{
    SoftmacMACLayer *smac = new SoftmacMACLayer;
    return smac;
}

void
SoftmacMACLayer::SetPacketRxSink(PacketEventSink* psink)
{
  click_chatter("SoftmacMACLayer: setting packetrx sink\n");
  _packetrxsink = psink;
}

void
SoftmacMACLayer::SetPacketTxDoneSink(PacketEventSink* psink)
{
  click_chatter("SoftmacMACLayer: setting packet txdone sink\n");
  _packettxdonesink = psink;
}

//
// These are the vmac_mac_t methods
//
int
SoftmacMACLayer::macinfo_packet_tx_done(void* me, struct sk_buff* thepacket, int intop)
{
  SoftmacMACLayer* obj = me;
  obj->_packettxdonesink->PacketEvent(Packet::make(thepacket),PacketEventSink::TXDONE);
  return VMAC_MAC_NOTIFY_OK;
}

int
SoftmacMACLayer::macinfo_packet_rx(void* me, struct sk_buff* thepacket, int intop)
{
  SoftmacMACLayer* obj = me;
  click_chatter("SoftmacMACLayer: cu_softmac_mac_packet_rx\n");
  obj->_packetrxsink->PacketEvent(Packet::make(thepacket),PacketEventSink::RX);
  return VMAC_MAC_NOTIFY_OK;
}


// Set the maclayer info struct to contain our shim functions
void
SoftmacMACLayer::macinfo_init(void *me, vmac_mac_t* macinfo)
{
    if (macinfo) {
	macinfo->mac_tx_done = macinfo_packet_tx_done;
	macinfo->rx_packet = macinfo_packet_rx;
	
	//macinfo->mac_tx = cu_softmac_mac_packet_tx;
	//macinfo->mac_work = cu_softmac_mac_work;
	//macinfo->attach_phy = cu_softmac_mac_attach;
	//macinfo->detach_phy = cu_softmac_mac_detach;
	//macinfo->set_rx_func = cu_softmac_mac_set_rx_func;
	//macinfo->set_unload_notify_func = cu_softmac_mac_set_unload_notify_func;
	
	macinfo->mac_private = me;
    }
}
CLICK_ENDDECLS
ELEMENT_PROVIDES(SoftmaMACLayer)

#include <click/config.h>
#include <click/confparse.hh>
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

#include "clicksmacphy.hh"
#include "tosmacphy.hh"
CLICK_DECLS

ToSMACPHY::ToSMACPHY()
  : Element(1, 0), _smacphy(0),_maxinflight(1)
{
}

ToSMACPHY::~ToSMACPHY()
{
}

int
ToSMACPHY::configure(Vector<String> &conf, ErrorHandler *errh)
{
  Element* smacel;
  if (cp_va_parse(conf, this, errh,
		  cpElement, "VMAC PHY element", &smacel,
		  cpOptional,
		  cpUnsigned, "maximum packets in flight", &_maxinflight,
		  cpEnd) < 0)
    return -1;
  
  if (smacel) {
    _smacphy = smacel->cast("ClickSMACPHYath");
  }

  if (!_smacphy) {
    return errh->error("unable to cast element to ClickSMACPHY");
  }

  return 0;
}

int
ToSMACPHY::initialize(ErrorHandler *errh)
{
  // XXX finish this
  return 0;
}

void
ToSMACPHY::cleanup(CleanupStage) {
  // XXX finish this
  
}

void
ToSMACPHY::push(int port, Packet *p)
{
  _smacphy->TransmitPacket(p,_maxinflight);  
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(linuxmodule ClickSMACPHY)
EXPORT_ELEMENT(ToSMACPHY)

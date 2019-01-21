#include "dsr-kmodule.h"

MODULE_AUTHOR("Alex Song <s369677@student.uq.edu.au>");
MODULE_DESCRIPTION("Dynamic Source Routing Kernel Module");

static char * ipaddr;
MODULE_PARM(ipaddr, "s");
static char * netmask = "255.255.255.0";
MODULE_PARM(netmask, "s");

/*
 * variables
 */
struct proc_dir_entry * proc_file;
static __u32 IPADDR;
static __u32 NETMASK;
static __u32 BCAST;
static __u32 NETWORK;

// struct needed to hook at pre route
static struct nf_hook_ops pre_route = {
  {NULL, NULL},
  pre_route_handler,
  PF_INET,
  NF_IP_PRE_ROUTING,
  NF_IP_PRI_MANGLE,
};
// struct needed to hook at local out
static struct nf_hook_ops local_out = {
  {NULL, NULL},
  local_out_handler,
  PF_INET,
  NF_IP_LOCAL_OUT,
  NF_IP_PRI_MANGLE,
};
// struct needed to hook at post route
static struct nf_hook_ops post_route = {
  {NULL, NULL},
  post_route_handler,
  PF_INET,
  NF_IP_POST_ROUTING,
  NF_IP_PRI_MANGLE,
};
// struct needed to hook at local in
static struct nf_hook_ops local_in = {
  {NULL, NULL},
  local_in_handler,
  PF_INET,
  NF_IP_LOCAL_IN,
  NF_IP_PRI_MANGLE,
};
static char debug_msg[500] = { 0 };

/*
 * functions
 */

/* Initialize the module - register the proc file */
int init_module()
{
  printk("DSR activated\n");

  /* calcualte network parameters */
  if (ipaddr != NULL)
    IPADDR = in_aton(ipaddr);

  if (netmask != NULL)
    NETMASK = in_aton(netmask);

  NETWORK = IPADDR & NETMASK;
  BCAST = IPADDR | ~NETMASK;

  printk("hook on\n");
  nf_register_hook(&pre_route);// it seems to always return 0
  nf_register_hook(&local_out);// it seems to always return 0
  nf_register_hook(&post_route);// it seems to always return 0
  nf_register_hook(&local_in);// it seems to always return 0

  printk("creating proc read entry\n");
  proc_file = create_proc_read_entry("dsrdebug", 0/*mode_t*/,
				     NULL, dsr_read_proc, NULL/*data*/);
  proc_file->size = 500;

  printk("success\n");  
  return 0;// 0 means module succesfully loaded
}

/* Cleanup - unregister everything */
void cleanup_module()
{
  printk("DSR shutdown\n");

  printk("hook off\n");
  nf_unregister_hook(&pre_route);// returns void
  nf_unregister_hook(&local_out);// returns void
  nf_unregister_hook(&post_route);// returns void
  nf_unregister_hook(&local_in);// returns void

  printk("remove proc entry\n");
  remove_proc_entry("dsrdebug", NULL);
}  

unsigned int local_in_handler(unsigned int hooknum,
			       struct sk_buff **skb,
			       const struct net_device *in,
			       const struct net_device *out,
			       int (*okfn)(struct sk_buff *)) {

  struct iphdr *iph = (*skb)->nh.iph;

  if ((iph->daddr & NETMASK) == NETWORK
      /*iph->protocol == DSR_PROTOCOL || iph->protocol == 1*/) {

    printk("\nlocal in enter\n");

    dump_skb_ptr(*skb);    
    dump_skb(PF_INET, *skb);
    
    printk("local in leave\n");
  }
  return NF_ACCEPT;
}

unsigned int post_route_handler(unsigned int hooknum,
			       struct sk_buff **skb,
			       const struct net_device *in,
			       const struct net_device *out,
			       int (*okfn)(struct sk_buff *)) {
  struct iphdr *iph = (*skb)->nh.iph;

  if ((iph->daddr & NETMASK) == NETWORK
      /*iph->protocol == DSR_PROTOCOL || iph->protocol == 1*/) {

    printk("\npost route enter\n");

    dump_skb_ptr(*skb);    
    dump_skb(PF_INET, *skb);
    
    printk("post route leave\n");
  }
  return NF_ACCEPT;
}

unsigned int local_out_handler(unsigned int hooknum,
			       struct sk_buff **skb,
			       const struct net_device *in,
			       const struct net_device *out,
			       int (*okfn)(struct sk_buff *)) {

  struct iphdr *iph = (*skb)->nh.iph;

  if ((iph->daddr & NETMASK) == NETWORK
      /*iph->protocol == DSR_PROTOCOL || iph->protocol == 1*/) {

    printk("\nlocal out enter\n");

    dump_skb_ptr(*skb);
    dump_skb(PF_INET, *skb);

    printk("local out leave\n");
  }

  return NF_ACCEPT;
}

unsigned int pre_route_handler(unsigned int hooknum,
			       struct sk_buff **skb,
			       const struct net_device *in,
			       const struct net_device *out,
			       int (*okfn)(struct sk_buff *)) {

  struct iphdr *iph = (*skb)->nh.iph;

  if ((iph->daddr & NETMASK) == NETWORK
      /* iph->protocol == DSR_PROTOCOL || iph->protocol == 1*/) {

    printk("\npre route enter\n");

    dump_skb_ptr(*skb);
    dump_skb(PF_INET, *skb);

    printk("pre route leave\n");
    return NF_ACCEPT;
    }

  return NF_ACCEPT;
}

/*
 * this function is copied from the linux source code fs/proc/proc_misc.c
 */
int proc_calc_metrics(char *page, char **start, off_t off,
				 int count, int *eof, int len)
{
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

int dsr_read_proc(char *page, char **start, off_t off, int count,
		  int *eof, void *data) {

  int len;
  /*
  if (off > 0) {
    return 0;
  }
  */

  len = sprintf(page, "%s", debug_msg);
  return proc_calc_metrics(page, start, off, count, eof, len);
}

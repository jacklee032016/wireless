#include <linux/sysctl.h>

#include "mesh_route.h"
#include "_route.h"

#ifdef CONFIG_SYSCTL
static int MESH_SYSCTL_DECL(mesh_sysctl_cleanup, ctl, write, filp, buffer,lenp, ppos)
{
	route_node_t *node = ctl->extra1;
//	int len = *lenp;
	u_int val;
	int ret = 0;

	ctl->data = &val;
	ctl->maxlen = sizeof(val);
	if (write)
	{
		ret = MESH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,	lenp, ppos);
		if (ret == 0)
			node->debug = val;
		node->mgmtOps->neigh_delete(node, NULL);
	}
#if 0	
	else
	{
		val = node->debug;
		ret = MESH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,	lenp, ppos);
	}
#endif	
	return ret;
}


static int MESH_SYSCTL_DECL(mesh_sysctl_debug, ctl, write, filp, buffer, lenp, ppos)
{
	route_node_t *node = ctl->extra1;
	u_int val;
	int ret;

	ctl->data = &val;
	ctl->maxlen = sizeof(val);
	if (write)
	{
		ret = MESH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,	lenp, ppos);
		if (ret == 0)
			node->debug = val;
	}
	else
	{
		val = node->debug;
		ret = MESH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer,	lenp, ppos);
	}
	return ret;
}

#define	CTL_AUTO	-2	/* cannot be CTL_ANY or CTL_NONE */

static const ctl_table mesh_sysctl_template[] = 
{
	{ 
		.ctl_name	= CTL_AUTO,
		.procname	= "debug",
		.mode		= 0644,
		.proc_handler	= mesh_sysctl_debug
	},
	{ 
		.ctl_name	= CTL_AUTO,
		.procname	= "clear",
		.mode		= 0644,
		.proc_handler	= mesh_sysctl_cleanup
	},
	{ 0 }
};

void mesh_sysctl_register(route_node_t *node)
{
	const char *cp;
	int i, space;

	space = 5*sizeof(struct ctl_table) + sizeof(mesh_sysctl_template);
	node->mesh_sysctls = kmalloc(space, GFP_KERNEL);
	if (node->mesh_sysctls == NULL)
	{
		printk("%s: no memory for sysctl table!\n", __func__);
		return;
	}

	/* setup the table */
	memset( node->mesh_sysctls, 0, space);
	node->mesh_sysctls[0].ctl_name = CTL_NET;
	node->mesh_sysctls[0].procname = "net";
	node->mesh_sysctls[0].mode = 0555;
	node->mesh_sysctls[0].child = &node->mesh_sysctls[2];
	/* [1] is NULL terminator */

	node->mesh_sysctls[2].ctl_name = CTL_AUTO;
	snprintf(node->mesh_procname, sizeof(node->name), "mesh" );
	node->mesh_sysctls[2].procname = node->mesh_procname;
	node->mesh_sysctls[2].mode = 0555;
	node->mesh_sysctls[2].child = &node->mesh_sysctls[4];
	/* [3] is NULL terminator */

	/* copy in pre-defined data */
	memcpy(&node->mesh_sysctls[4], mesh_sysctl_template,	sizeof(mesh_sysctl_template));

	/* add in dynamic data references */
	for (i = 4; node->mesh_sysctls[i].ctl_name; i++)
		if (node->mesh_sysctls[i].extra1 == NULL)
			node->mesh_sysctls[i].extra1 = node;

	/* and register everything */
	node->mesh_sysctl_header = register_sysctl_table(node->mesh_sysctls, 1);
	if (!node->mesh_sysctl_header)
	{
		printk("%s: failed to register sysctls!\n", node->mesh_procname);
		kfree(node->mesh_sysctls);
		node->mesh_sysctls = NULL;
	}
}
// EXPORT_SYMBOL(mesh_sysctl_register);

void mesh_sysctl_unregister(route_node_t *node)
{
	if (node->mesh_sysctl_header)
	{
		unregister_sysctl_table(node->mesh_sysctl_header);
		node->mesh_sysctl_header = NULL;
	}
	if (node->mesh_sysctls)
	{
		kfree(node->mesh_sysctls);
		node->mesh_sysctls = NULL;
	}
}
// EXPORT_SYMBOL(mesh_sysctl_unregister);
#endif


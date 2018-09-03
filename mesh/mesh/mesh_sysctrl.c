/*
* $Id: mesh_sysctrl.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
*/

#include "mesh.h"

#ifdef CONFIG_SYSCTL
static int MESH_SYSCTL_DECL(mesh_sysctl_cleanup, ctl, write, filp, buffer,lenp, ppos)
{
	MESH_MGR *mgr = ctl->extra1;
//	int len = *lenp;
	u_int val;
	int ret = 0;

	ctl->data = &val;
	ctl->maxlen = sizeof(val);
	if (write)
	{
		ret = MESH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos);
		if (ret == 0)
			mgr->debug = val;
//		mgr->mgmtOps->neigh_delete(mgr, NULL);
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
	MESH_MGR *mgr = ctl->extra1;
	u_int val;
	int ret;

	ctl->data = &val;
	ctl->maxlen = sizeof(val);
	if (write)
	{
		ret = MESH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos);
		if (ret == 0)
			mgr->debug = val;
	}
	else
	{
		val = mgr->debug;
		ret = MESH_SYSCTL_PROC_DOINTVEC(ctl, write, filp, buffer, lenp, ppos);
	}
	return ret;
}

#define	CTL_AUTO	-2	/* cannot be CTL_ANY or CTL_NONE */

static const ctl_table mesh_sysctl_template[] = 
{
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "debug",
		.mode		= 0644,
		.proc_handler	= mesh_sysctl_debug
	},
	{ 
		.ctl_name	= SWJTU_MESH_SYSCTRL_DEV_AUTO,
		.procname	= "clear",
		.mode		= 0644,
		.proc_handler	= mesh_sysctl_cleanup
	},
	{ 0 }
};

void swjtu_mesh_sysctl_register(MESH_MGR *mgr)
{
	const char *cp;
	int i, space;

	space = 5*sizeof(struct ctl_table) + sizeof(mesh_sysctl_template);
	mgr->meshSysctls = kmalloc(space, GFP_KERNEL);
	if ( mgr->meshSysctls == NULL)
	{
		MESH_WARN_INFO("%s: no memory for sysctl table!\n", __func__);
		return;
	}

	/* setup the table */
	memset( mgr->meshSysctls, 0, space);
#if 0	
	mgr->meshSysctls[0].ctl_name = CTL_NET;
	mgr->meshSysctls[0].procname = "net";
#else
	mgr->meshSysctls[0].ctl_name = SWJTU_MESH_SYSCTRL_ROOT_DEV;
	mgr->meshSysctls[0].procname = SWJTU_MESH_DEV_NAME;
#endif
	mgr->meshSysctls[0].mode = 0555;
	mgr->meshSysctls[0].child = &mgr->meshSysctls[2];
	/* [1] is NULL terminator */

	mgr->meshSysctls[2].ctl_name = CTL_AUTO;
#if 0	
	snprintf( mgr->meshProcname, sizeof(mgr->name), "mesh" );
#else
	snprintf( mgr->meshProcname, sizeof(mgr->name), SWJTU_MESH_DEV_NAME);
#endif
	mgr->meshSysctls[2].procname = mgr->meshProcname;
	mgr->meshSysctls[2].mode = 0555;
	mgr->meshSysctls[2].child = &mgr->meshSysctls[4];
	/* [3] is NULL terminator */

	/* copy in pre-defined data */
	memcpy(&mgr->meshSysctls[4], mesh_sysctl_template,	sizeof(mesh_sysctl_template));

	/* add in dynamic data references */
	for (i = 4; mgr->meshSysctls[i].ctl_name; i++)
		if (mgr->meshSysctls[i].extra1 == NULL)
			mgr->meshSysctls[i].extra1 = mgr;

	/* and register everything */
	mgr->meshSysctlHeader = register_sysctl_table( mgr->meshSysctls, 1);
	if (!mgr->meshSysctlHeader)
	{
		MESH_WARN_INFO("%s: failed to register sysctls!\n", mgr->meshProcname);
		kfree(mgr->meshSysctls);
		mgr->meshSysctls = NULL;
	}
}

void swjtu_mesh_sysctl_unregister(MESH_MGR *mgr)
{
	if (mgr->meshSysctlHeader)
	{
		unregister_sysctl_table(mgr->meshSysctlHeader);
		mgr->meshSysctlHeader = NULL;
	}
	if ( mgr->meshSysctls)
	{
		kfree(mgr->meshSysctls);
		mgr->meshSysctls = NULL;
	}
}
// EXPORT_SYMBOL(mesh_sysctl_unregister);
#endif


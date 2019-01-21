/* 
 * $Log: as_misc_init.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
 * rebuild
 *
 * Revision 1.4  2005/11/30 07:23:23  wangwei
 * 增加烧录93LC66程序，可以通过条件WITH_93LC66=no选择编译
 *
 * Revision 1.3  2005/09/07 10:02:00  wangwei
 * add drivers of button
 *
 * Revision 1.2  2005/06/15 02:57:27  wangwei
 * 增加了蜂鸣器用户程序接口
 *
 * Revision 1.1  2005/06/07 09:16:21  lizhijie
 * add into CVS
 *
 * $Id: as_misc_init.c,v 1.1.1.1 2006/11/29 09:16:54 lizhijie Exp $
 * driver for Miscellious device in our IXP4xx board, such as WatchDog and beep
 * as_misc_init.c  : initial interface of different misc driver module
 * Li Zhijie, 2005.06.07
*/
#include <linux/config.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>

#include  "as_misc_dev.h"
#include "as_misc_version.h"


int as_watchdog_proc_write(struct file *file, const char *buf, unsigned long count, void *data);
int as_watchdog_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data);
void as_ixp_watchdog_disable(void );

#if HARDWARE_93LC66
int as_93lc66_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data);
int as_93lc66_proc_write(struct file *file, const char *buf, unsigned long count, void *data);
#endif
int as_beep_proc_write(struct file *file, const char *buf, unsigned long count, void *data);
int as_beep_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data);
int as_button_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data);
int as_button_proc_write(struct file *file, const char *buf, unsigned long count, void *data);
as_misc_dev  misc_devs[] = 
{
	{
		name			:	AS_MISC_PROC_WATCHDOG,
		as_proc_entry		:	NULL,
		read_handler		:	as_watchdog_proc_read,
		write_handler		:	as_watchdog_proc_write
	},
	
	{
		name			:	AS_MISC_PROC_BEEP,
		as_proc_entry		:	NULL,
		read_handler		:	as_beep_proc_read,
		write_handler		:	as_beep_proc_write
	},

	{
		name			:	AS_MISC_PROC_BUTTON,
		as_proc_entry		:	NULL,
		read_handler		:	as_button_proc_read,
		write_handler		:	as_button_proc_write
#if HARDWARE_93LC66
	},
	
	{
		name			:	AS_MISC_PROC_93LC66,
		as_proc_entry		:	NULL,
		read_handler		:	as_93lc66_proc_read,
		write_handler		:	as_93lc66_proc_write
#endif
	}
};


static struct proc_dir_entry *as_misc_root_dir;



static void __exit as_misc_devs_cleanup(void) 
{
	int i;
//	char proc_name[128];

	as_ixp_watchdog_disable();
	for(i=0; i< (sizeof(misc_devs)/sizeof(as_misc_dev) ); i++ )
	{
		if(misc_devs[i].as_proc_entry)
		{
//			sprintf(proc_name, "%s/%s", AS_MISC_PROC_DIR_NAME, misc_devs[i].name);
			remove_proc_entry(misc_devs[i].name,  as_misc_root_dir );
		}
	}

	if(as_misc_root_dir)
		remove_proc_entry(AS_MISC_PROC_DIR_NAME,  NULL);
	
	printk(KERN_INFO "Assist Driver for WatchDog Unloaded\r\n" );
}

static int __init as_misc_devs_init(void) 
{
	int res = 0;
	int i;
//	char proc_name[128];

	as_misc_root_dir = proc_mkdir(AS_MISC_PROC_DIR_NAME , NULL);
	if( as_misc_root_dir == NULL)
	{
		res = -ENOMEM;
		goto Error;
	}
	as_misc_root_dir->owner = THIS_MODULE;
		

	for(i=0; i< (sizeof(misc_devs)/sizeof(as_misc_dev) ); i++ )
	{
//		sprintf(proc_name, "%s/%s", AS_MISC_PROC_DIR_NAME, misc_devs[i].name );
		misc_devs[i].as_proc_entry = create_proc_entry(misc_devs[i].name, 0644, as_misc_root_dir );
		if(misc_devs[i].as_proc_entry == NULL)
		{
			res = -ENOMEM;
			goto Error;
		}
		misc_devs[i].as_proc_entry->owner = THIS_MODULE;
		misc_devs[i].as_proc_entry->read_proc = misc_devs[i].read_handler;
		misc_devs[i].as_proc_entry->write_proc = misc_devs[i].write_handler;
		misc_devs[i].as_proc_entry->data = &( misc_devs[i]) ;

	}
	
	printk(KERN_INFO  AS_VERSION_INFO("assist_misc") );
	return res;

Error:
	as_misc_devs_cleanup();
	return res;
}


MODULE_AUTHOR("Chengdu R&D <support@assistcn.com>");
MODULE_DESCRIPTION("Assist WatchDog Chip Interface");
MODULE_LICENSE("GPL");


module_init( as_misc_devs_init);
module_exit( as_misc_devs_cleanup );


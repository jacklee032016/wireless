/*
 * $Log: as_misc_dev.h,v $
 * Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
 * rebuild
 *
 * Revision 1.4  2005/11/30 07:21:29  wangwei
 * 增加烧录93LC66部分程序
 *
 * Revision 1.3  2005/09/07 10:02:51  wangwei
 * no message
 *
 * Revision 1.2  2005/06/15 02:56:52  wangwei
 * 增加了蜂鸣器相关函数的申明
 *
 * Revision 1.1  2005/06/07 09:16:41  lizhijie
 * add into CVS
 *
 * $Id: as_misc_dev.h,v 1.1.1.1 2006/11/29 09:16:54 lizhijie Exp $
 * assist misc device defination is declared here
*/
#ifndef  __AS_MISC_DEV_H__
#define __AS_MISC_DEV_H__

#if  __KERNEL__

#include <linux/proc_fs.h>

#define AS_NAME_LENGTH			64
typedef struct
{
	char name[AS_NAME_LENGTH];		/* used as proc file name, such as watchdog */
	
	struct proc_dir_entry *as_proc_entry;

	int (*read_handler)(char *page, char **start, off_t off, int count, int *eof, void *data);
	int (*write_handler)(struct file *file, const char *buf, unsigned long count, void *data);
	
}as_misc_dev;

#endif

typedef enum
{
	AS_WATCHDOG_ENABLE	=	1,
	AS_WATCHDOG_UPDATE,
	AS_WATCHDOG_DISABLE
}AS_WATCHDOG_CMD_TYPE;

typedef struct 
{
	AS_WATCHDOG_CMD_TYPE		type;
	unsigned int				 	value;		/* for example, the period of warm-reset */
}AS_WATCHDOG_COMMAND;

typedef enum
{
	AS_BEEP_ENABLE	=	1,
	AS_BEEP_DISABLE
}AS_BEEP_CMD_TYPE;

typedef struct 
{
	AS_BEEP_CMD_TYPE		type;
	unsigned int				 	value;		
}AS_BEEP_COMMAND;
typedef enum
{
	AS_BUTTON_STATE	=	1
}AS_BUTTON_CMD_TYPE;
typedef struct 
{
	AS_BUTTON_CMD_TYPE			type;
	unsigned int				 	value;		
}AS_BUTTON_COMMAND;
typedef enum
{
	AS_93LC66_WRITE = 1,
		AS_93LC66_READ
}AS_93LC66_CMD_TYPE;
typedef struct 
{
	AS_93LC66_CMD_TYPE			type;
	unsigned int				 	value;		
}AS_93LC66_COMMAND;
#define AS_IXP_WATCHDOG_DEFAULT_COUNT				0XFFFFFFFE

#define  AS_MISC_PROC_DIR_NAME	"as_misc"		
#define  AS_MISC_PROC_WATCHDOG	"watchdog"
#define  AS_MISC_PROC_BEEP			"beep"
#define  AS_MISC_PROC_BUTTON		"button"
#define AS_MISC_PROC_93LC66		"93lc66"
#endif


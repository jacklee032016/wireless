
#include "valoha.h"

typedef struct
{
	const char* name;

	mode_t mode;
	int entryid;
} valoha_proc_entry_t;

enum
{
	VALOHA_PROC_TXBITRATE,
	VALOHA_PROC_DEFERTX,
	VALOHA_PROC_DEFERTXDONE,
	VALOHA_PROC_DEFERRX,
	VALOHA_PROC_MAXINFLIGHT,

	/* Allow user to turn encryption on or off */
	VALOHA_PROC_USECRYPTO,

	VALOHA_PROC_COUNT
};

static const valoha_proc_entry_t aloha_proc_entries[] =
{
	{
		"txbitrate",
		0644,
		VALOHA_PROC_TXBITRATE
	},
	{
		"defertx",
		0644,
		VALOHA_PROC_DEFERTX
	},
	{
		"defertxdone",
		0644,
		VALOHA_PROC_DEFERTXDONE
	},
	{
		"deferrx",
		0644,
		VALOHA_PROC_DEFERRX
	},
	{
		"maxinflight",
		0644,
		VALOHA_PROC_MAXINFLIGHT
	},
	{
		"usecrypto",
		0644,
		VALOHA_PROC_USECRYPTO
	},
	{
		0,  0, -1
	},
};

/**
 *  An instance of this data structure is created for each proc
 * filesystem entry and placed into a linked list associated
 * with each instance. This allows us to handle proc filesystem
 * read/write requests.
 */
typedef struct
{
	struct list_head list;
	valoha_inst_t* inst;
	int entryid;
	char name[CHEESYMAC_PROCDIRNAME_LEN];
	struct proc_dir_entry* parentdir;  
} VALOHA_PROC_DATA;

/* Root directory for cheesymac procfs entries */
struct proc_dir_entry* aloha_proc_handle = 0;


int valoha_delete_procfs(valoha_inst_t* inst) 
{
	int result = 0;
	if (inst)
	{
		struct list_head* tmp = 0;
		struct list_head* p = 0;
		VALOHA_PROC_DATA* proc_entry_data = 0;

		/* Remove individual entries and delete their data */
		list_for_each_safe(p,tmp,&inst->my_procfs_data)
		{
			proc_entry_data = list_entry(p,VALOHA_PROC_DATA,list);
			list_del(p);
			remove_proc_entry(proc_entry_data->name,proc_entry_data->parentdir);
			kfree(proc_entry_data);
			proc_entry_data = 0;
		}
	}
	return result;
}

static int __valoha_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int result = 0;
	VALOHA_PROC_DATA* procdata = data;
	if (procdata && procdata->inst)
	{
		valoha_inst_t* inst = procdata->inst;
		char* dest = (page + off);
		int intval = 0;

		switch (procdata->entryid)
		{
			case VALOHA_PROC_TXBITRATE:
				read_lock(&(inst->mac_busy));
				intval = inst->txbitrate;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			case VALOHA_PROC_DEFERTX:
				read_lock(&(inst->mac_busy));
				intval = inst->defertx;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;
				break;

			case VALOHA_PROC_DEFERTXDONE:
				read_lock(&(inst->mac_busy));
				intval = inst->defertxdone;
				read_unlock(&(inst->mac_busy));
				result = snprintf(dest,count,"%d\n",intval);
				*eof = 1;

			break;

		case VALOHA_PROC_DEFERRX:
			read_lock(&(inst->mac_busy));
			intval = inst->deferrx;
			read_unlock(&(inst->mac_busy));
			result = snprintf(dest,count,"%d\n",intval);
			*eof = 1;
			break;

		case VALOHA_PROC_MAXINFLIGHT:
			read_lock(&(inst->mac_busy));
			intval = inst->maxinflight;
			read_unlock(&(inst->mac_busy));
			result = snprintf(dest,count,"%d\n",intval);
			*eof = 1;
			break;

		/* CODEX STEP 4 BEGIN
		* Add a read handler for the proc file entry
		*/
		/** Allow user to check encryption status  */

		case VALOHA_PROC_USECRYPTO:
			read_lock(&(inst->mac_busy));
			intval = inst->use_crypto;
			read_unlock(&(inst->mac_busy));
			result = snprintf(dest,count,"%d\n",intval);
			*eof = 1;
			break;
		/* CODEX STEP 4 END  */
		default:
		/* Unknown entry -- do something benign  */
			result = 0;
			*eof = 1;
			break;
		}
	}

	return result;
}

static int __valoha_write_proc(struct file *file, const char __user *buffer,  unsigned long count, void *data)
{
	int result = 0;
	VALOHA_PROC_DATA* procdata = data;
	if (procdata && procdata->inst)
	{
		valoha_inst_t* inst = procdata->inst;
		static const int maxkdatalen = 256;
		char kdata[maxkdatalen];
		char* endp = 0;
		long intval = 0;

		if (maxkdatalen <= count)
		{
			copy_from_user(kdata,buffer,(maxkdatalen-1));
			kdata[maxkdatalen-1] = 0;
			result = (maxkdatalen-1);
		}
		else
		{
			copy_from_user(kdata,buffer,count);
			result = count;
		}

		switch (procdata->entryid)
		{
			case VALOHA_PROC_TXBITRATE:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->txbitrate = intval;
				write_unlock(&(inst->mac_busy));
				break;

			case VALOHA_PROC_DEFERTX:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->defertx = intval;
				write_unlock(&(inst->mac_busy));
				break;

			case VALOHA_PROC_DEFERTXDONE:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->defertxdone = intval;
				write_unlock(&(inst->mac_busy));
				break;

			case VALOHA_PROC_DEFERRX:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->deferrx = intval;
				write_unlock(&(inst->mac_busy));
				break;

			case VALOHA_PROC_MAXINFLIGHT:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->maxinflight = intval;
				write_unlock(&(inst->mac_busy));
				break;

			/* Allow user turn encryption on or off */
			case VALOHA_PROC_USECRYPTO:
				intval = simple_strtol(kdata,&endp,10);
				write_lock(&(inst->mac_busy));
				inst->use_crypto = intval;
				write_unlock(&(inst->mac_busy));
				break;

			default:
				break;
		}
	}
	else
	{
		result = count;
	}
	return result;
}

int valoha_make_procfs(valoha_inst_t* inst) 
{
	int result = 0;

	if (inst)
	{
		int i = 0;
		struct proc_dir_entry* curprocentry = 0;
		VALOHA_PROC_DATA* curprocdata = 0;
		vmac_mac_t *macinfo = inst->mymac;

		/* root directory created by softmac_core */
		inst->my_procfs_dir = macinfo->proc;

		/* Make individual entries. Stop when we get either a null string
		* or an empty string for a name.   */
		i = 0;
		while ( aloha_proc_entries[i].name && aloha_proc_entries[i].name[0])
		{
			//printk(KERN_ALERT "CheesyMAC: Creating proc entry %s, number %d\n",cheesymac_inst_proc_entries[i].name,i);
			curprocentry = create_proc_entry(aloha_proc_entries[i].name,  aloha_proc_entries[i].mode,  inst->my_procfs_dir);
			curprocentry->owner = THIS_MODULE;

			/* Allocate and fill out a proc data structure, add it
			* to the linked list for the instance. */
			curprocdata = kmalloc(sizeof(VALOHA_PROC_DATA),GFP_ATOMIC);
			INIT_LIST_HEAD((&curprocdata->list));
			list_add_tail(&(curprocdata->list),&(inst->my_procfs_data));
			curprocdata->inst = inst;
			curprocdata->entryid = aloha_proc_entries[i].entryid;
			strncpy(curprocdata->name, aloha_proc_entries[i].name,CHEESYMAC_PROCDIRNAME_LEN);
			curprocdata->parentdir = inst->my_procfs_dir;

			/* Hook up the new proc entry data */
			curprocentry->data = curprocdata;

			/* Set read/write functions for the proc entry. */
			curprocentry->read_proc = __valoha_read_proc;
			curprocentry->write_proc = __valoha_write_proc;
			i++;
		}
	}

	return result;
}


/*
 * $Author: lizhijie $
 * $Log: as_tel_init.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.4  2005/07/07 02:47:56  wangwei
 * 更改设备号从0-7
 *
 * Revision 1.3  2005/04/26 06:03:29  lizhijie
 * add version info in /proc/asstel/span
 *
 * Revision 1.2  2005/04/20 02:35:37  lizhijie
 * no message
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.5  2004/12/11 05:38:16  lizhijie
 * add Tiger320 debug info
 *
 * Revision 1.4  2004/11/29 01:55:25  lizhijie
 * update proc file output
 *
 * Revision 1.3  2004/11/26 12:33:51  lizhijie
 * add multi-card support
 *
 * Revision 1.2  2004/11/22 01:54:04  lizhijie
 * add some user module into CVS
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"


#define CONFIG_PROC_FS		1
#include <linux/proc_fs.h>

#define  AS_PROC_DIR_NAME	"astel"
#define AS_CHAR_DEV_NAME	"astel"
#define AS_PROC_NAME_LENGTH	32
#if AS_DEBUG_TIGER
/* added by lizhijie 2004.12.10, for debug card info */
#define AS_PROC_CARD_NAME		"module"
#endif

//static int debug;

extern struct as_dsp_dev assist_tone_dsp;

int as_file_open(struct inode *inode, struct file *file);
int  as_file_release(struct inode *inode, struct file *file);
int as_file_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data);
ssize_t as_file_read(struct file *file, char *usrbuf, size_t count, loff_t *ppos);
ssize_t as_file_write(struct file *file, const char *usrbuf, size_t count, loff_t *ppos);
unsigned int as_file_poll(struct file *file, struct poll_table_struct *wait_table);

static struct file_operations as_file_ops = 
{
	owner: 	THIS_MODULE,
	llseek: 	NULL,
	open: 	as_file_open,
	release: 	as_file_release,
	ioctl: 	as_file_ioctl,
	read: 	as_file_read,
	write: 	as_file_write,
	poll: 	as_file_poll,
	mmap: 	NULL,
	flush: 	NULL,
	fsync: 	NULL,
	fasync: 	NULL,
};

struct as_dev_span master_span =
{
	name	:	"Assist Telephone Interface",
	desc		:	"Assist Telephone Framework for Linux(Chengdu R&D <support@assistcn.com>)",
	count	:	0,
	chans	:	NULL,
	private	:	NULL,		/* point to wcfxs list , lizhijie 2004.11.25 */
	flags		:	AS_FLAG_RBS,
	zone		:	NULL
};

static rwlock_t chan_lock = RW_LOCK_UNLOCKED;

static struct proc_dir_entry *as_proc_root_entry;

static struct proc_dir_entry *as_proc_span_entry;
static struct proc_dir_entry *as_proc_entries[AS_MAX_CHANNELS]; 

static struct proc_dir_entry *as_proc_dsp_entry; 

#if AS_DEBUG_TIGER
/* proc file for wcfxs, eg. card info , added by lizhijie 2004.12.08 */
static struct proc_dir_entry *as_proc_module_entry; 

static int __as_tel_proc_read_module(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	char *info;

	if (off > 0)
		return 0;
//	trace
		
	info = as_cards_debug_status_info( data);
	
	if(!info)
		len += sprintf(page + len, "No memory available\r\n");
	else	
	{
		len += sprintf(page + len, "%s\r\n", info);
		kfree(info);
		info = NULL;
	}
	return len;
}
#endif

static int __as_tel_proc_read_channel(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	struct as_dev_chan	*chan;
	char *info;

	if (off > 0)
		return 0;
//	trace
		
	chan = (struct as_dev_chan *)data;
	if (!chan)
		return 0;

	info = as_channel_debug_info( chan);
	
	if(!info)
		len += sprintf(page + len, "No memory available\r\n");
	else	
	{
		len += sprintf(page + len, "%s\r\n", info);
		kfree(info);
		info = NULL;
	}
	return len;
}


static int __as_tel_proc_read_span(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	char *info;
	struct as_dev_span  	*span;
	struct as_dev_chan	*chan;

	if (off > 0)
		return 0;
//	trace
		
	span = (struct as_dev_span *)data;
	if (!span)
		return 0;

	if ( span->name ) 
	{
		len += sprintf(page + len, "Assist Telephone Interface : " );
		len += sprintf(page + len,  AS_VERSION_INFO("span") );
	}
//		len += sprintf(page + len, "Assist Telephone Interface : %s\r\n", span->name );
	if ( span->desc )
		len += sprintf(page + len, "'%s'\r\n", span->desc );
	else
		len += sprintf(page + len, "\r\n\r\n");
#if 0
	len += sprintf(page + len, "alarm status : \"%d\"\r\n", span->alarms );
	len += sprintf(page + len, "BPV count : \"%d\"\r\n", span->bpvcount );
	len += sprintf(page + len, "rx level : \"%d\"\r\n", span->rxlevel );
	len += sprintf(page + len, "tx level : \"%d\"\r\n", span->txlevel );
	len += sprintf(page + len, "CRC4 error count : \"%d\"\r\n", span->crc4count );
	len += sprintf(page + len, "E-bit error count : \"%d\"\r\n", span->ebitcount );
	len += sprintf(page + len, "FAS error count  : \"%d\"\r\n", span->fascount );
	len += sprintf(page + len, "IRQ miss count : \"%d\"\r\n", span->irqmisses );
	len += sprintf(page + len, "active sync source  : \"%d\"\r\n", span->syncsrc );
	len += sprintf(page + len, "Total Channel : \"%d\"\r\n", span->channels );

	len += sprintf(page + len, " \r\n");
#endif
	len += sprintf(page + len, "Cards Info : \r\n"  );
	info = as_cards_debug_info(span->private);
	if(!info)
		len += sprintf(page + len, "No memory available\r\n");
	else	
		len += sprintf(page + len, "%s\r\n", info);

	len += sprintf(page + len, "\r\nChannels Info : \r\n" );
	chan = span->chans;
/* every channel in this span */
	while ( chan ) 
	{	
		if ( chan->span ) 
		{
			info = as_channel_debug_info( chan);
	
			if(!info)
				len += sprintf(page + len, "No memory available\r\n");
			else	
				len += sprintf(page + len, "%s\r\n", info);
	
		}
		chan = chan->next;
	}
#if 0
	len += sprintf(page + len, "Total %d channels have registered in\r\n" , span->count );
#endif
	if(span->dsp)
	{
		len += sprintf(page + len, "\r\nDSP engine info : '%s'\r\n" , (span->dsp->get_desc)(span->dsp) );
		len += sprintf(page + len, "\tTones for '%s' is used in DSP now\r\n" , span->dsp->zone->data->name );
	}
	
	
	return len;
}

struct as_dev_chan  *as_span_get_channel(int channo)
{
	struct as_dev_chan *ch;

	ch = master_span.chans;
	while(ch)
	{
		if(ch->channo == channo /*&& ch->flags ==*/ )
			return ch;
		ch = ch->next;
	};

	return NULL;
}

struct as_dsp_dev  *as_span_get_dsp(void )
{
	return  master_span.dsp;
}


/* after registered, this channel is closed */
int as_channel_register(struct as_dev_chan *chan)
{
	struct as_dev_chan *_ch, *last = NULL;
	unsigned long flags;
	char tempfile[AS_PROC_NAME_LENGTH];
	int res = 0;
	int i;
	
	write_lock_irqsave(&chan_lock, flags);
	
	_ch = master_span.chans;
	
	while(_ch)
	{
		if(_ch == chan ||!strcmp( _ch->name,  chan->name) )
		{
			printk("Device %s has exist now\r\n", chan->name );
			return -EEXIST;
		}	
		last = _ch;
		_ch = _ch->next;
	};

	spin_lock_init(&chan->lock);
	if(!last)
		master_span.chans = chan;
	else	
		last->next = chan;
	
	
	chan->channo = master_span.count ;
	master_span.count ++;
	chan->span = &master_span;

	chan->dsp = master_span.dsp;

	if (!chan->readchunk)
		chan->readchunk = chan->sreadchunk;
	if (!chan->writechunk)
		chan->writechunk = chan->swritechunk;
	as_channel_set_law(chan, 0);
				
	as_close_channel(chan); 
	chan->flags |= AS_CHAN_FLAG_REGISTERED;
	chan->flags |= AS_CHAN_FLAG_AUDIO;
	chan->flags |= AS_CHAN_FLAG_RUNNING;
	
	for(i=0; i< AS_MAX_CADENCE; i++)
		chan->ringcadence[i] = master_span.zone->data->ringcadence[i];
	//	zt_hangup(chans[chan->channo]);

	write_unlock_irqrestore(&chan_lock, flags); 
	
	printk("ASSIST TELEPHONE : %d channels are available\n", master_span.count );

	sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, chan->name);
	as_proc_entries[chan->channo] = create_proc_read_entry(tempfile, 0444, NULL , __as_tel_proc_read_channel, (int *)(long)chan );
	
	/* added for multi-card suppport , lizhjie 2004.11.25 */
	if( (!master_span.private) &&  chan->private )
	{
		master_span.private = chan->private;
#if AS_DEBUG_TIGER
		sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, AS_PROC_CARD_NAME );
		as_proc_module_entry = create_proc_read_entry(tempfile, 0444, NULL , __as_tel_proc_read_module,  master_span.private );
#endif
	}
	
	return res;
}


void as_channel_unregister(struct as_dev_chan *chan)
{
	struct as_dev_chan *_ch, *last = NULL;
	unsigned long flags;
	char tempfile[AS_PROC_NAME_LENGTH];
	
	write_lock_irqsave(&chan_lock, flags);
	if (chan->flags & AS_CHAN_FLAG_REGISTERED) 
	{
		chan->flags &= ~AS_CHAN_FLAG_REGISTERED;
	}

	_ch = master_span.chans;
	last = _ch;

	while(_ch ) 
	{
		if(_ch==chan && !strcmp(_ch->name, chan->name ) )
		{
		/* Registered channels are static data defined in low layer driver, so no free needed  */
			if(last==_ch)
				master_span.chans = _ch->next;
			else
				last->next = chan->next;
				
			master_span.count --;
			sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, chan->name );
			remove_proc_entry(tempfile, NULL);

			kfree(chan);
			chan = NULL;
			break;
		}
		last = _ch;
		_ch = _ch->next;
	}	

	/* added for multi-cards support ,lizhijie 2004.11.25 */
	if(master_span.count ==0)
	{
#if AS_DEBUG_TIGER
		sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, AS_PROC_CARD_NAME );
		remove_proc_entry(tempfile, NULL);
#endif
		master_span.private = NULL;
	}
	
	write_unlock_irqrestore(&chan_lock, flags);
}

static int __init as_init(void) 
{
	char tempfile[AS_PROC_NAME_LENGTH];
	int res = 0;

	as_proc_root_entry = proc_mkdir(AS_PROC_DIR_NAME , NULL);
	
	sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, "span" );
	as_proc_span_entry = create_proc_read_entry(tempfile, 0444, NULL , __as_tel_proc_read_span, (int *)(long)&master_span );

	if ((res = register_chrdev(AS_DEV_MAJOR,  AS_CHAR_DEV_NAME, &as_file_ops))) 
	{
		printk(KERN_ERR "Unable to register tor device '%s' on %d\n", AS_CHAR_DEV_NAME, AS_DEV_MAJOR);
		return res;
	}

	printk(KERN_INFO "Assist Telephony Interface Registered on major %d\n", AS_DEV_MAJOR);
	spin_lock_init(&master_span.lock);
	as_law_conv_tables_init();

	if( (assist_tone_dsp.init)(&assist_tone_dsp, &master_span) )
		return -EFAULT;
	
	sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, "dsp" );
	as_proc_dsp_entry = create_proc_read_entry(tempfile, 0444, NULL , assist_tone_dsp.proc_read, (int *)(long)&assist_tone_dsp );

	rwlock_init(&chan_lock);

	printk( AS_VERSION_INFO("span") );
	
	return res;
}

static void __exit as_cleanup(void) 
{
	char tempfile[AS_PROC_NAME_LENGTH];
	sprintf(tempfile, "%s/%s", AS_PROC_DIR_NAME, "span" );

	remove_proc_entry(tempfile, NULL);

	remove_proc_entry(AS_PROC_DIR_NAME,  NULL);

	printk(KERN_INFO "Assist Telephony Interface Unloaded\n");
	unregister_chrdev(AS_DEV_MAJOR,  AS_CHAR_DEV_NAME );

	if(master_span.dsp)
		(master_span.dsp->destory)(master_span.dsp );

}


EXPORT_SYMBOL(as_channel_register);
EXPORT_SYMBOL(as_channel_unregister);
EXPORT_SYMBOL(as_io_span_out_transmit);
EXPORT_SYMBOL(as_io_span_in_receive);
EXPORT_SYMBOL(as_io_channel_hooksig);
EXPORT_SYMBOL(as_channel_qevent_lock);
EXPORT_SYMBOL(as_channel_hangup);

EXPORT_SYMBOL(as_ec_chunk);


MODULE_AUTHOR("Chengdu R&D <support@assistcn.com>");
MODULE_DESCRIPTION("Assist Telephony Interface");
MODULE_LICENSE("GPL");


module_init( as_init);
module_exit( as_cleanup );


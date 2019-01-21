/*
 * $Author: lizhijie $
 * $Log: as_span_ctl.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/11/29 08:25:03  eagle
 * 2229 by chenchaoxin
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
#include "asstel.h"
extern struct as_dev_span master_span ;

unsigned int as_span_poll(struct file *file, struct poll_table_struct *wait_table)
{

	return -EINVAL;
}

ssize_t as_span_read(struct file *file, char *usrbuf, size_t count, loff_t *ppos)
{

	return -EINVAL;
}

ssize_t as_span_write(struct file *file, const char *usrbuf, size_t count, loff_t *ppos)
{

	return -EINVAL;
}


/* no operation in open mode for /dev/astel/span is needed */
int as_span_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;
	return 0;
}

int as_span_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;
	return 0;
}


static int as_ioctl_load_zone(unsigned long data)
{
	struct as_zone_tones_data *zone_tones;
	struct as_tone *samples[MAX_TONES];
	int next[MAX_TONES];
	struct as_tone_def_header th;
	void *slab, *ptr;
	long size;
	struct as_tone_def td;
	struct as_tone *t;
	int x;
	int space;
	int res;
	struct as_dsp_dev *dsp;

	dsp = as_span_get_dsp();
	if(!dsp)
	{
		printk("DSP device has not initialized now\n");
		return -EINVAL;
	}
	
	memset(samples, 0, sizeof(samples));
	memset(next, 0, sizeof(next));

	copy_from_user(&th, (struct as_tone_def_header *)data, sizeof(th));
	if ((th.count < 0) || (th.count > MAX_TONES)) 
	{
		printk("Too many tones included in %s\n", th.name);
		return -EINVAL;
	}
	
	space = size = sizeof(struct as_zone_tones_data) + th.count * sizeof(struct as_tone);
	if ((size > MAX_SIZE) || (size < 0))
		return -E2BIG;
	
	ptr = slab = (char *)kmalloc(size, GFP_KERNEL);
	if (!slab)
		return -ENOMEM;
	memset(slab, 0, size);

	zone_tones = (struct as_zone_tones_data *)slab;
	strncpy(zone_tones->name, th.name, sizeof(zone_tones->name) - 1);
	zone_tones->zone_id = th.zone_id;
	for (x=0;x<AS_MAX_CADENCE;x++)
		zone_tones->ringcadence[x] = th.ringcadence[x];
	
	data += sizeof(struct as_tone_def_header);
	ptr += sizeof(struct as_zone_tones_data);
	space -= sizeof(struct as_zone_tones_data);
	
	for (x=0;x<th.count;x++) 
	{
		if (space < sizeof(struct as_tone)) 
		{
			kfree(slab);
			printk("Insufficient tone zone space\n");
			return -EINVAL;
		}
		
		if (copy_from_user(&td, (struct as_tone_def *)data, sizeof(struct as_tone_def))) 
		{
			kfree(slab);
			return -EIO;
		}
		
		samples[x] = t = (struct as_tone *)ptr;
		next[x] = td.next;
		if ((next[x] >= th.count) || (next[x] < 0)) 
		{
			printk("Invalid 'next' pointer\n");
			kfree(slab);
			return -EINVAL;
		}
		
		if (td.tone_id >= AS_TONE_MAX) 
		{
			printk("Too many tones defined\n");
			kfree(slab);
			return -EINVAL;
		}
		
		/* Update pointers to account for zt_tone header */
		space -= sizeof(struct as_tone);
		ptr += sizeof(struct as_tone);
		data += sizeof(struct as_tone_def);

		t->tonesamples = td.samples;
		t->fac1 = td.fac1;
		t->init_v2_1 = td.init_v2_1;
		t->init_v3_1 = td.init_v3_1;
		t->fac2 = td.fac2;
		t->init_v2_2 = td.init_v2_2;
		t->init_v3_2 = td.init_v3_2;
		t->modulate = td.modulate;

		t->raw_tone.freq1 = td.raw_tone.freq1;
		t->raw_tone.freq2 = td.raw_tone.freq2;
		t->raw_tone.time = td.raw_tone.time;

		t->next = NULL;
		
		if (!zone_tones->tones[td.tone_id])
			zone_tones->tones[td.tone_id] = t;
	}
	
	for (x=0;x<th.count;x++) 
		samples[x]->next = samples[next[x]];

	res = (dsp->replace_tones)(dsp, zone_tones);
	if (res)
		kfree(slab);

	return res;
}


int  as_span_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long data)
{
		int i;	
		struct as_dev_chan *ch=master_span.chans;
		struct as_channel_state chanel_states;
		switch(cmd)
		{
			case AS_CTL_LOADZONE:
				return as_ioctl_load_zone(data);
			case AS_GET_CHANNEL_NUMBER:
				i=master_span.count;
				if (copy_to_user((int*)data, &i, sizeof(i)))
					return -EIO;
				break;
			case AS_GET_CHANNELS_STATES:
				
			#if 0	
			chanel_states=(struct as_channel_state *)kmalloc(8*sizeof(struct as_channel_state), GFP_KERNEL);
			if(chanel_states==NULL)
			{
				printk("can't alloc enough memory for the channel states\n");
				return 0;
			}
			else
			{
				while(_ch)
				{
					chanel_states->channel_no=_ch.channol;
					if(_ch.name[2]=='S')
					chanel_states->type=AS_CHANNEL_TYPE_FXS;
					else
						chanel_states->type=AS_CHANNEL_TYPE_FXO;
					if(_ch.txstate== AS_TXSTATE_ONHOOK)
						chanel_states->available=0;
					else
						chanel_states->available=1;
					*chanel_states++;
					i++;
					_ch=_ch->next;							
				}
				if(i <8)
				{
					chanel_states->channel_no=0;
					chanel_states->available=-1;
					chanel_states->type=0;			
				}
				*chanel_states-=i;
				if (copy_to_user((struct as_channel_state *)data, &chanel_states, sizeof(8*sizeof(struct as_channel_state))))
					return -EIO;
				kfree(chanel_states);
			#endif	
			if (copy_from_user(&chanel_states, (struct as_channel_state *)data, sizeof(chanel_states)))
				return -EIO;
			i=chanel_states.channel_no;
			while(i > 1)
				{
				ch=ch->next;
				i--;
				}
			if((ch->sigcap&__AS_SIG_FXS)==1 /*||ch->name[2]=='S'*/)
				chanel_states.type=AS_CHANNEL_TYPE_FXS;
			else
				chanel_states.type=AS_CHANNEL_TYPE_FXO;
			if(ch->txstate== AS_TXSTATE_ONHOOK)
				chanel_states.available=0;
			else
				chanel_states.available=1;
			if (copy_to_user((struct as_channel_state *)data, &chanel_states, sizeof(chanel_states)))
				return -EIO;
			
			break;
			default:
				printk("Command '%d' is not implemented in span device now\r\n", cmd);
				return 0;
		}

	return 0;
}


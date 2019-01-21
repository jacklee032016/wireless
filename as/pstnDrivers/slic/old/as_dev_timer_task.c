static void rh_init_int_timer(voice_card *pvoice)
{
		init_timer (&pvoice->rh_int_timer);
		pvoice->rh_int_timer.function = rh_int_timer_do;
		pvoice->rh_int_timer.data = (unsigned long) pvoice;
		pvoice->rh_int_timer.expires = jiffies + (HZ *50) / 1000;
		add_timer (&pvoice->rh_int_timer);
}

static void voice_irq(int irq, void *dev_id, struct pt_regs * regs)
{	int count = 0;	u32 stat,astat;	struct voice_card *pvoice = dev_id;

	return;}

static void rh_int_timer_do(unsigned long ptr){	struct voice_card *pvoice = (struct voice_card *)ptr;
	int i;
	
	down(pvoice->lock);
	for(i=0;i<2;i++)
		vpPollingProcess(i);
	up(pvoice->lock);

	rh_init_int_timer(pvoice);
}

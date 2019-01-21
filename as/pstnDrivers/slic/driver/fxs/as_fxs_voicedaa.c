/*
 * $Author: lizhijie $
 * $Revision: 1.1.1.1 $
 * $Log: as_fxs_voicedaa.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.2  2004/11/25 07:59:24  lizhijie
 * recommit all
 *
*/
#include "as_fxs_common.h"
extern int debug;
extern struct fxo_mode  fxo_modes[];
extern int _opermode ;

static int __wcfxs_voicedaa_insane(struct wcfxs *wc, int card)
{
	int blah;
	blah = as_fxs_getreg(wc, card, 2);
	if (blah != 0x3)
		return -2;
	blah = as_fxs_getreg(wc, card, 11);
	if (debug)
		printk("VoiceDAA System: %02x\n", blah & 0xf);
	return 0;
}



int as_fxs_init_voicedaa(struct wcfxs *wc, int card, int fast, int manual, int sane)
{
	long newjiffies;
	unsigned char reg16=0, reg26=0, reg30=0, reg31=0;
	
	wc->modtype[card] = MOD_TYPE_FXO;
	/* Sanity check the ProSLIC */
	as_reset_spi(wc, card);
	if (!sane && __wcfxs_voicedaa_insane(wc, card))
		return -2;

	/* Software reset */
	as_fxs_setreg(wc, card, 1, 0x80);

	/* Wait just a bit */
	newjiffies = jiffies + (HZ/10);
	while(jiffies < newjiffies);

	/* Enable PCM, ulaw */
	as_fxs_setreg(wc, card, 33, 0x28);

	/* Set On-hook speed, Ringer impedence, and ringer threshold */
#if AS_WITH_FXO_MODULE
	reg16 |= (fxo_modes[_opermode].ohs << 6);
	reg16 |= (fxo_modes[_opermode].rz << 1);
	reg16 |= (fxo_modes[_opermode].rt);
#endif

	as_fxs_setreg(wc, card, 16, reg16);
	
	/* Set DC Termination:
	   Tip/Ring voltage adjust, minimum operational current, current limitation */
#if AS_WITH_FXO_MODULE
	reg26 |= (fxo_modes[_opermode].dcv << 6);
	reg26 |= (fxo_modes[_opermode].mini << 4);
	reg26 |= (fxo_modes[_opermode].ilim << 1);
#endif

	as_fxs_setreg(wc, card, 26, reg26);

	/* Set AC Impedence */
#if AS_WITH_FXO_MODULE
	reg30 = (fxo_modes[_opermode].acim);
#endif
	as_fxs_setreg(wc, card, 30, reg30);

	/* Misc. DAA parameters */
	reg31 = 0xa3;
#if AS_WITH_FXO_MODULE
	reg31 |= (fxo_modes[_opermode].ohs2 << 3);
#endif
	as_fxs_setreg(wc, card, 31, reg31);

	/* Set Transmit/Receive timeslot */
	as_fxs_setreg(wc, card, 34, (3-card) * 8);
	as_fxs_setreg(wc, card, 35, 0x00);
	as_fxs_setreg(wc, card, 36, (3-card) * 8);
	as_fxs_setreg(wc, card, 37, 0x00);

	/* Enable ISO-Cap */
	as_fxs_setreg(wc, card, 6, 0x00);

	/* Wait 1000ms for ISO-cap to come up */
	newjiffies = jiffies;
	newjiffies += 2 * HZ;
	while((jiffies < newjiffies) && !(as_fxs_getreg(wc, card, 11) & 0xf0));

	if (!(as_fxs_getreg(wc, card, 11) & 0xf0)) 
	{
		printk("VoiceDAA did not bring up ISO link properly!\n");
		return -1;
	}
	if (debug)
		printk("ISO-Cap is now up, line side: %02x rev %02x\n", 
		       as_fxs_getreg(wc, card, 11) >> 4,
		       (as_fxs_getreg(wc, card, 13) >> 2) & 0xf);
	/* Enable on-hook line monitor */
	as_fxs_setreg(wc, card, 5, 0x08);
	return 0;
		
}


/*
 * $Id: asISDNManufacturer.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * definition of AS_ISDN own Manufacturer functions
 *
 */

#ifndef AS_ISDNManufacturer_H
#define AS_ISDNManufacturer_H

#define AS_ISDN_MANUFACTURER_ID	0x44534963	/* "mISD" */

/* AS_ISDN_MANUFACTURER message layout
 *
 * Controller		dword	Controller Address
 * ManuID		dword	AS_ISDN_MANUFACTURER_ID
 * Class		dword   Function Class
 * Function		dword	Function Identifier
 * Function specific	struct	Data for this Function
 *
 * in a CONF the Function specific struct contain at least
 * a word which is coded as Capi Info word (error code)
 */
 
/*
 * HANDSET special functions
 *
 */
#define AS_ISDN_MF_CLASS_HANDSET				1

#define AS_ISDN_MF_HANDSET_ENABLE			1	/* no function specific data */
#define AS_ISDN_MF_HANDSET_DISABLE			2	/* no function specific data */
#define AS_ISDN_MF_HANDSET_SETMICVOLUME		3	/* word volume value */
#define AS_ISDN_MF_HANDSET_SETSPKVOLUME		4	/* word volume value */
#define AS_ISDN_MF_HANDSET_GETMICVOLUME		5	/* CONF: Info, word volume value */
#define AS_ISDN_MF_HANDSET_GETSPKVOLUME		6	/* CONF: Info, word volume value */

#endif /* AS_ISDNManufactor_H */

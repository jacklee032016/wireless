/*
* $Id: ah_devid.h,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#ifndef _DEV_ATH_DEVID_H_
#define _DEV_ATH_DEVID_H_

#define ATHEROS_VENDOR_ID	0x168c		/* Atheros PCI vendor ID */
/*
 * NB: all Atheros-based devices should have a PCI vendor ID
 *     of 0x168c, but some vendors, in their infinite wisdom
 *     do not follow this so we must handle them specially.
 */
#define	ATHEROS_3COM_VENDOR_ID	0xa727		/* 3Com 3CRPAG175 vendor ID */
#define	ATHEROS_3COM2_VENDOR_ID	0x10b7		/* 3Com 3CRDAG675 vendor ID */

/* AR5210 (for reference) */
#define AR5210_DEFAULT          0x1107          /* No eeprom HW default */
#define AR5210_PROD             0x0007          /* Final device ID */
#define AR5210_AP               0x0207          /* Early AP11s */

/* AR5211 */
#define AR5211_DEFAULT          0x1112          /* No eeprom HW default */
#define AR5311_DEVID            0x0011          /* Final ar5311 devid */
#define AR5211_DEVID            0x0012          /* Final ar5211 devid */
#define AR5211_LEGACY           0xff12          /* Original emulation board */
#define AR5211_FPGA11B          0xf11b          /* 11b emulation board */

/* AR5212 */
#define AR5212_DEFAULT          0x1113          /* No eeprom HW default */
#define AR5212_DEVID            0x0013          /* Final ar5212 devid */
#define AR5212_FPGA             0xf013          /* Emulation board */
#define	AR5212_DEVID_IBM	0x1014          /* IBM minipci ID */
#define AR5212_AR5312_REV2      0x0052          /* AR5312 WMAC (AP31) */
#define AR5212_AR5312_REV7      0x0057          /* AR5312 WMAC (AP30-040) */
#define AR5212_AR2313_REV8      0x0058          /* AR2313 WMAC (AP43-030) */

/* AR5212 compatible devid's also attach to 5212 */
#define	AR5212_DEVID_0014	0x0014
#define	AR5212_DEVID_0015	0x0015
#define	AR5212_DEVID_0016	0x0016
#define	AR5212_DEVID_0017	0x0017
#define	AR5212_DEVID_0018	0x0018
#define	AR5212_DEVID_0019	0x0019
#define AR5212_AR2413      	0x001a          /* AR2413 aka Griffin-lite */

/* AR5213 */
#define	AR5213_SREV_1_0		0x0055
#define	AR5213_SREV_REG		0x4020

#define	AR_SUBVENDOR_ID_NOG	0x0e11		/* No 11G subvendor ID */
#define AR_SUBVENDOR_ID_NEW_A	0x7065		/* Update device to new RD */
#endif /* _DEV_ATH_DEVID_H */
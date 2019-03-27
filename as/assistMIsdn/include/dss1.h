/*
 * $Id: dss1.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 * DSS1 (Euro) D-channel protocol defines
 */

#ifndef l3dss1_process

#define T302		15000
#define T303		4000
#define T304		30000
#define T305		30000
#define T308		4000
/* for layer 1 certification T309 < layer1 T3 (e.g. 4000) */
/* This makes some tests easier and quicker */
#define T309		40000
#define T310		30000
#define T313		4000
#define T318		4000
#define T319		4000
#define N303		1
#define T_CTRL	180000

/* private TIMER events */
#define CC_T302		0x030201
#define CC_T303		0x030301
#define CC_T304		0x030401
#define CC_T305		0x030501
#define CC_T308_1	0x030801
#define CC_T308_2	0x030802
#define CC_T309         0x030901
#define CC_T310		0x031001
#define CC_T313		0x031301
#define CC_T318		0x031801
#define CC_T319		0x031901
#define CC_TCTRL		0x031f01

/* Message-Types, Refer to Specs. Vol.3, P.62  */
/* call establishment message */
#define MT_ALERTING					0x01
#define MT_CALL_PROCEEDING			0x02
#define MT_CONNECT					0x07
#define MT_CONNECT_ACKNOWLEDGE	0x0f
#define MT_PROGRESS				0x03
#define MT_SETUP					0x05
#define MT_SETUP_ACKNOWLEDGE		0x0d	/* not offered in INS NET */
/* call info phase messgae */
#define MT_RESUME					0x26
#define MT_RESUME_ACKNOWLEDGE	0x2e
#define MT_RESUME_REJECT			0x22
#define MT_SUSPEND					0x25
#define MT_SUSPEND_ACKNOWLEDGE	0x2d
#define MT_SUSPEND_REJECT			0x21
#define MT_USER_INFORMATION		0x20	/*Not offered in INS */
/* call clearing message */
#define MT_DISCONNECT				0x45
#define MT_RELEASE					0x4d
#define MT_RELEASE_COMPLETE		0x5a
#define MT_RESTART					0x46	
#define MT_RESTART_ACKNOWLEDGE	0x4e	
/* other message */
#define MT_SEGMENT					0x60	/* not offered in INS */
#define MT_CONGESTION_CONTROL		0x79	/* not Offered in iNS */
#define MT_INFORMATION				0x7b
#define MT_FACILITY					0x62	/* not defined in INS */
#define MT_NOTIFY					0x6e
#define MT_STATUS					0x7d
#define MT_STATUS_ENQUIRY			0x75

/* Info Element, Refer to Specs, vol.3, P.66 */
#define IE_SEGMENT			0x00		/* 4.5.26 not offered in INS, see  */
#define IE_BEARER			0x04		/* 4.5.5 bearer capability, */
#define IE_CAUSE			0x08		/* 4.5.12 */
#define IE_CALL_ID			0x10		/* 4.5.6, used in Suspend/Resume*/
#define IE_CALL_STATE		0x14		/* 4.5.7 */
#define IE_CHANNEL_ID		0x18		/* 4.5.13 */
#define IE_FACILITY			0x1c		/* not defined in INS */
#define IE_PROGRESS			0x1e		/* 4.5.23 */
#define IE_NET_FAC			0x20		/* 4.5.21 Network specific Facility, not Offered in INS */
#define IE_NOTIFY			0x27		/* 4.5.22 */
#define IE_DISPLAY			0x28		/* 4.5.16 */
#define IE_DATE				0x29		/* 4.5.15. Not Offer in INS */
#define IE_KEYPAD			0x2c		/* 4.5.18, keypad_facility */
#define IE_SIGNAL			0x34		/* 4.5.28 */
#define IE_INFORATE			0x40		/*  */
#define IE_E2E_TDELAY		0x42
#define IE_TDELAY_SEL		0x43
#define IE_PACK_BINPARA	0x44
#define IE_PACK_WINSIZE	0x45
#define IE_PACK_SIZE		0x46
#define IE_CUG				0x47
#define IE_REV_CHARGE		0x4a
#define IE_CONNECT_PN		0x4c
#define IE_CONNECT_SUB		0x4d
#define IE_CALLING_PN		0x6c		/* 4.5.10 */
#define IE_CALLING_SUB		0x6d		/* 4.5.11 */
#define IE_CALLED_PN		0x70		/* 4.5.8 */
#define IE_CALLED_SUB		0x71		/* 4.5.9 */
#define IE_REDIR_NR			0x74
#define IE_TRANS_SEL		0x78		/* 4.5.29, not offered in INS. Transit Network Selection */
#define IE_RESTART_IND		0x79		/* 4.5.25 */
#define IE_LLC				0x7c		/* 4.5.19 Low Layer Compatibility */
#define IE_HLC				0x7d		/* 4.5.17, High Layer Compatibility */
#define IE_USER_USER		0x7e		/* 4.5.30 */
#define IE_ESCAPE			0x7f		/* 4.5.1 */
/* single octet IE */
#define IE_SHIFT				0x90		/* 4.5.3 for codeset */
#define IE_MORE_DATA		0xa0		/* 4.5.20. not offered in INS */
#define IE_COMPLETE			0xa1		/* 4.5.27 not offered in INS */
#define IE_CONGESTION		0xb0		/* 4.5.14 not offered in INS */
#define IE_REPEAT			0xd0		/* 4.5.24 */

#define IE_MANDATORY		0x0100
/* mandatory not in every case */
#define IE_MANDATORY_1		0x0200

#define ERR_IE_COMPREHENSION	 		1
#define ERR_IE_UNRECOGNIZED			-1
#define ERR_IE_LENGTH					-2
#define ERR_IE_SEQUENCE				-3

#define CAUSE_LOC_USER					0

#define CAUSE_NORMAL_CLEARING			16
#define CAUSE_CALL_REJECTED			21
#define CAUSE_INVALID_NUMBER			28
#define CAUSE_STATUS_RESPONSE			30
#define CAUSE_NORMALUNSPECIFIED		31
#define CAUSE_TEMPORARY_FAILURE		41
#define CAUSE_RESOURCES_UNAVAIL 		47
#define CAUSE_INVALID_CALLREF			81
#define CAUSE_MANDATORY_IE_MISS		96
#define CAUSE_MT_NOTIMPLEMENTED		97
#define CAUSE_IE_NOTIMPLEMENTED		99
#define CAUSE_INVALID_CONTENTS		100
#define CAUSE_NOTCOMPAT_STATE		101
#define CAUSE_TIMER_EXPIRED			102
#define CAUSE_PROTOCOL_ERROR			111

#define NO_CAUSE		254

#else /* only l3dss1_process */

/* l3dss1 specific data in l3 process */
typedef struct
{
	unsigned char invoke_id; /* used invoke id in remote ops, 0 = not active */
	ulong ll_id; /* remebered ll id */
	u_char remote_operation; /* handled remote operation, 0 = not active */ 
	int proc; /* rememered procedure */  
	ulong remote_result; /* result of remote operation for statcallb */
	char uus1_data[35]; /* data send during alerting or disconnect */
} dss1_proc_priv;

/* l3dss1 specific data in protocol stack */
typedef struct
{
	unsigned char last_invoke_id; /* last used value for invoking */
	unsigned char invoke_used[32]; /* 256 bits for 256 values */
} dss1_stk_priv;        

#endif /* only l3dss1_process */

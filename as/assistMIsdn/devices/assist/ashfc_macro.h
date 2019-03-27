/*
 * $Id: ashfc_macro.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */
 
/* NOTE: MAX_PORTS must be 4*MAX_CARDS for 4S card*/
#define MAX_CARDS							1
#define MAX_PORTS							4

#define ASHFC_CHIP_TYPE_4S					4
#define ASHFC_CHIP_TYPE_8S					8

#define ASHFC_RAM_32K						0
#define ASHFC_RAM_128K						1
#define ASHFC_RAM_512K						2

#define ASHFC_TXPENDING_NO				0
#define ASHFC_TXPENDING_YES				1
#define ASHFC_TXPENDING_SPLLOOP			2


#define TE_STATE_F1_INACTIVE				1
#define TE_STATE_F2_SENSING				2
#define TE_STATE_F3_DEACTIVATED			3
#define TE_STATE_F4_AWAITING_SIGNAL		4
#define TE_STATE_F5_IDENTIFY_INPUT		5
#define TE_STATE_F6_SYNCHRONIZED			6
#define TE_STATE_F7_ACTIVATED				7
#define TE_STATE_F8_LOST_FRAMEING		8

#define NT_STATE_G1_DEACTIVATED			1
#define NT_STATE_G2_PEND_ACTIVATION		2
#define NT_STATE_G3_ACTIVATED				3
#define NT_STATE_G4_PEND_DEACTIVATION	4

/* refer to p.173 */
#define TX_FIFO_CONN_FIFO2PCM				0x20
#define TX_FIFO_CONN_FIFO2ST				0x80
#define TX_FIFO_CONN_ST2PCM				0xC0

#define RX_FIFO_CONN_FIFO2ST				0x80	/* same as 0x00 */
#define RX_FIFO_CONN_ST2PCM				0xC0

/* NT protocol  */
#define ASISDN_NT_D_PROTOCOL_STACK		(ISDN_PID_L0_NT_S0 | \
											ISDN_PID_L1_NT_S0 )

#define ASISDN_NT_L2_D_PROTOCOL_STACK		(ISDN_PID_L0_NT_S0 | \
											ISDN_PID_L1_NT_S0  \
											|ISDN_PID_L2_LAPD_NET )


#define ASISDN_NT_B_PROTOCOL_STACK		(ISDN_PID_L0_NT_S0 | \
											 ISDN_PID_L1_B_64TRANS  \
											|ISDN_PID_L2_B_TRANS )
/*
											// \
											//|ISDN_PID_L2_B_TRANS|ISDN_PID_L3_B_TRANS)
*/

/* TE protocol */
#define ASISDN_TE_D_PROTOCOL_STACK		(ISDN_PID_L0_TE_S0|\
											ISDN_PID_L1_TE_S0 |\
											ISDN_PID_L2_LAPD | \
											ISDN_PID_L3_INS_USER)
/* DF_PTP only used in NT mode
|ISDN_PID_L2_DF_PTP|ISDN_PID_L3_DF_PTP
*/

#define ASISDN_TE_B_PROTOCOL_STACK		(ISDN_PID_L0_TE_S0|ISDN_PID_L1_B_64TRANS \
											|ISDN_PID_L2_B_TRANS )

//|ISDN_PID_L3_B_TRANS )

// |ISDN_PID_L3_B_TRANS)


#define AS_ISDN_TE_DEV_LAYERS			( ISDN_LAYER(0)|ISDN_LAYER(1) | \
										 ISDN_LAYER(2)  |ISDN_LAYER(3)  )


#define AS_ISDN_NT_DEV_LAYERS			( ISDN_LAYER(0)|ISDN_LAYER(1) )

#define AS_ISDN_NT_L2_DEV_LAYERS			( ISDN_LAYER(0)|ISDN_LAYER(1)|ISDN_LAYER(2)  )

/*
//| \
//										ISDN_LAYER(2) )
*/

extern int ashfc_debug;
extern int ashfc_poll;
extern int ashfc_poll_timer;
extern as_isdn_object_t	ASHFC_obj;
extern u_char ashfc_silence;


/* main */
int lock_dev(void *data, int nowait);
void unlock_dev(void *data);
void ashfc_dbusy_timer(ashfc_t *hc);
void ashfc_pcm(ashfc_t *hc, int ch, int slot_tx, int bank_tx, int slot_rx, int bank_rx);
void ashfc_conf(ashfc_t *hc, int ch, int num);
void ashfc_splloop(ashfc_t *hc, int ch, u_char *data, int len);
int ashfc_sel_free_B_chan(ashfc_t *hc, int ch, channel_info_t *ci);

/* signal */
int ashfc_l1hw(as_isdn_if_t *hif, struct sk_buff *skb);
int ashfc_l2l1(as_isdn_if_t *hif, struct sk_buff *skb);

/* bottom half */
void ashfc_bch_bh(bchannel_t *bch);
void ashfc_dch_bh(dchannel_t *dch);

/* irq */
void ashfc_leds(ashfc_t *hc);
void ashfc_rx(ashfc_t *hc, int ch, dchannel_t *dch, bchannel_t *bch);
void ashfc_tx(ashfc_t *hc, int ch, dchannel_t *dch, bchannel_t *bch);
irqreturn_t ashfc_interrupt(int intno, void *dev_id, struct pt_regs *regs);

/* HW init */
int ashfc_hw_init( ashfc_t *hc);
int ashfc_setup_pci(ashfc_t *hc);
void ashfc_release_io(ashfc_t *hc);

/* HW utils */
int ashfc_chan_mode(ashfc_t *hc, int ch, int protocol, int slot_tx, int bank_tx, int slot_rx, int bank_rx);

/* init */
void ashfc_release_port(ashfc_t *hc, int port);

/* init_utils */
int  ashfc_init_object(as_isdn_object_t	*obj);
int  ashfc_init_ashfc(as_isdn_object_t *obj, ashfc_t *hc);



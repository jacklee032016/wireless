/*
 * $Id: l3.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include "asISDN.h"
#include <linux/skbuff.h>
#include "fsm.h"
#ifdef AS_ISDN_MEMDEBUG
#include "memdbg.h"
#endif

#define SBIT(state) 		(1<<state)
#define ALL_STATES  		0x03ffffff

#define PROTO_DIS_EURO	0x08	/* Protocol Discriminator for Q.931 */

#define L3_DEB_WARN	0x01
#define L3_DEB_PROTERR	0x02
#define L3_DEB_STATE	0x04
#define L3_DEB_CHARGE	0x08
#define L3_DEB_CHECK	0x10
#define L3_DEB_SI		0x20

#define FLG_L2BLOCK		1
#define FLG_PTP			2
#define FLG_EXTCID		3	/* extended Call ID */
#define FLG_CRLEN2		4	/* CR: Call Ref */

typedef struct _L3Timer 
{
	struct _l3_process		*pc;
	struct timer_list		tl;
	int					event;
} L3Timer_t;

/* represent a call control proces */
typedef struct _l3_process 
{
	struct list_head	list;
	struct _layer3	*l3;
	int				callref;
	int				state;
	L3Timer_t		timer;
	int				n303;
	struct sk_buff	*t303skb;
	u_int			id;
	int				bc;			/* which B Channel is used by this call */
	int				err;			/* keep CAUSE info in this field */
} l3_process_t;

/* layer3 management entity */
typedef struct _layer3
{
	struct list_head		list;
	struct FsmInst		l3m;			/* FSM for L3_LC_stateMachine */
	struct FsmTimer		l3m_timer;
	int					entity;
	int					pid_cnt;
	int					next_id;			/* maintain L3 ID for all l3_process_t */
	struct list_head		plist;			/* l3_process_t list */
	l3_process_t			*global;
	l3_process_t			*dummy;
	int					(*p_mgr)(l3_process_t *, u_int, void *);	/* handler for l3_process_t in Layer3 management entity,point to l3_process_mgr  */
	int					down_headerlen;
	u_int				id;
	int					debug;
	u_long				Flag;
	as_isdn_instance_t		inst;
	struct sk_buff_head	squeue;
	spinlock_t              	lock;
	int					OrigCallRef;		/* maintainer for CallRef value */
} layer3_t;

struct stateentry
{
	int		state;
	u_int	primitive;
	void		(*rout) (l3_process_t *, u_char, void *);
};

extern int		l3_msg(layer3_t *, u_int, int, int, void *);
extern void		newl3state(l3_process_t *, int);
extern void		L3InitTimer(l3_process_t *, L3Timer_t *);
extern void		L3DelTimer(L3Timer_t *);
extern int		L3AddTimer(L3Timer_t *, int, int);
extern void		StopAllL3Timer(l3_process_t *);
extern void		release_l3_process(l3_process_t *);
extern l3_process_t	*getl3proc(layer3_t *, int);
extern l3_process_t	*getl3proc4id(layer3_t *, u_int);
extern l3_process_t	*new_l3_process(layer3_t *, int, int, u_int);
extern u_char		*findie(u_char *, int, u_char, int);
extern int		AS_ISDN_l3up(l3_process_t *, u_int, struct sk_buff *);
extern int		getcallref(u_char * p);
extern int		newcallref(layer3_t *);

extern void		AS_ISDNl3New(void);
extern void		AS_ISDNl3Free(void);
#if 0
extern void		l3_debug(layer3_t *, char *, ...);
#endif

/*added after 2005.08.24*/
extern int l3_process_mgr(l3_process_t *, u_int, void *);

extern void l3ins_status_send(l3_process_t *pc, u_char cause);

extern struct sk_buff *MsgStart(l3_process_t *pc, u_char mt, int len);
extern int SendMsg(l3_process_t *pc, struct sk_buff *skb, int state);

extern void l3ins_msg_without_setup(l3_process_t *pc, u_char cause);
extern void l3ins_message_cause(l3_process_t *pc, u_char mt, u_char cause);
extern int l3ins_message(l3_process_t *pc, u_char mt);

#include "helper.h"
#if 0
#include "debug.h"
#endif

#include "dss1.h"


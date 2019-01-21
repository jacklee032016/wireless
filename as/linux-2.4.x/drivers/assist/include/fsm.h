/*
 * $Id: fsm.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#ifndef _AS_ISDN_FSM_H
#define _AS_ISDN_FSM_H

#include <linux/timer.h>

/* Statemachine */

struct FsmInst;

typedef void (* FSMFNPTR)(struct FsmInst *, int, void *);

struct Fsm
{
	FSMFNPTR *jumpmatrix;
	int state_count, event_count;
	char **strEvent, **strState;
};

struct FsmInst
{
	struct Fsm *fsm;
	int state;
#if AS_ISDN_DEBUG_FSM
	int debug;
#endif
	void *userdata;
	int userint;
#if AS_ISDN_DEBUG_FSM	
	void (*printdebug) (struct FsmInst *, char *, ...);
#endif
};

struct FsmNode
{
	int state, event;
	void (*routine) (struct FsmInst *, int, void *);
};

struct FsmTimer 
{
	struct FsmInst *fi;
	struct timer_list tl;
	int event;
	void *arg;
};

extern void as_isdn_fsm_new(struct Fsm *, struct FsmNode *, int);
extern void as_isdn_fsm_free(struct Fsm *);
extern int as_isdn_fsm_event(struct FsmInst *, int , void *);
extern void fsm_change_state(struct FsmInst *, int);
extern void fsm_init_timer(struct FsmInst *, struct FsmTimer *);
extern int fsm_add_timer(struct FsmTimer *, int, int, void *, int);
extern void fsm_restart_timer(struct FsmTimer *, int, int, void *, int);
extern void fsm_del_timer(struct FsmTimer *, int);

#endif

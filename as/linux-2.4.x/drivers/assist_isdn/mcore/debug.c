/*
 * $Id: debug.c,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

#include <linux/module.h>
#include <linux/asISDNif.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include "debug.h"

#define AS_ISDN_STATUS_BUFSIZE 4096

static char tmpbuf[AS_ISDN_STATUS_BUFSIZE];

void vAS_ISDN_debug(int id, char *head, char *fmt, va_list args)
{
/* if head == NULL the fmt contains the full info */
	char *p = tmpbuf;

	p += sprintf(p,"%d ", id);
	if (head)
		p += sprintf(p, "%s ", head);
	p += vsprintf(p, fmt, args);
	printk(KERN_ERR  "%s\n", tmpbuf);
} 

void AS_ISDN_debug(int id, char *head, char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vAS_ISDN_debug(id, head, fmt, args);
	va_end(args);
}

void AS_ISDN_debugprint(AS_ISDNinstance_t *inst, char *fmt, ...)
{
	logdata_t log;

	va_start(log.args, fmt);
	log.head = inst->name;
	log.fmt = fmt;
	inst->obj->ctrl(inst, MGR_DEBUGDATA | REQUEST, &log);
	va_end(log.args);
}

char *AS_ISDN_getrev(const char *revision)
{
	char *rev;
	char *p;

	if ((p = strchr(revision, ':'))) {
		rev = p + 2;
		p = strchr(rev, '$');
		if (p)
			*--p = 0;
	} else
		rev = "???";
	return rev;
}

int AS_ISDN_QuickHex(char *txt, u_char * p, int cnt)
{
	register int i;
	register char *t = txt;
	register u_char w;

	for (i = 0; i < cnt; i++) {
		*t++ = ' ';
		w = (p[i] >> 4) & 0x0f;
		if (w < 10)
			*t++ = '0' + w;
		else
			*t++ = 'A' - 10 + w;
		w = p[i] & 0x0f;
		if (w < 10)
			*t++ = '0' + w;
		else
			*t++ = 'A' - 10 + w;
	}
	*t++ = 0;
	return (t - txt);
}

EXPORT_SYMBOL(vAS_ISDN_debug);
EXPORT_SYMBOL(AS_ISDN_debug);
EXPORT_SYMBOL(AS_ISDN_getrev);
EXPORT_SYMBOL(AS_ISDN_debugprint);
EXPORT_SYMBOL(AS_ISDN_QuickHex);

/*
 * $Id: debug.h,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
 */

extern void vAS_ISDN_debug(int id, char *head, char *fmt, va_list args);
extern void AS_ISDN_debug(int id, char *head, char *fmt, ...);
extern char * AS_ISDN_getrev(const char *revision);
extern void AS_ISDN_debugprint(as_isdn_instance_t *inst, char *fmt, ...);
extern int AS_ISDN_QuickHex(char *, u_char *, int);

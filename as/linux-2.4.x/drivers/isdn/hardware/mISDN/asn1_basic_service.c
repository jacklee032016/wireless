/* $Id: asn1_basic_service.c,v 1.1.1.1 2006/11/29 08:55:19 lizhijie Exp $
 *
 */

#include "asn1.h"
#include "asn1_generic.h"
#include "asn1_basic_service.h"

// ======================================================================
// Basic Service Elements EN 300 196-1 D.6

int ParseBasicService(struct asn1_parm *pc, u_char *p, u_char *end, int *basicService)
{
	return ParseEnum(pc, p, end, basicService);
}


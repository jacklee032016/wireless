/*
* $Id: rc4.c,v 1.1.1.1 2006/11/30 17:00:40 lizhijie Exp $
 */
 
#include <linux/types.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>

#include "rc4.h"

static __inline void
swap_bytes(u_char *a, u_char *b)
{
	u_char temp;

	temp = *a;
	*a = *b;
	*b = temp;
}

/*
 * Initialize an RC4 state buffer using the supplied key,
 * which can have arbitrary length.
 */
void
rc4_init(struct rc4_state *const state, const u_char *key, int keylen)
{
	u_char j;
	int i;

	/* Initialize state with identity permutation */
	for (i = 0; i < 256; i++)
		state->perm[i] = (u_char)i; 
	state->index1 = 0;
	state->index2 = 0;
  
	/* Randomize the permutation using key data */
	for (j = i = 0; i < 256; i++) {
		j += state->perm[i] + key[i % keylen]; 
		swap_bytes(&state->perm[i], &state->perm[j]);
	}
}
EXPORT_SYMBOL(rc4_init);

/*
 * Encrypt some data using the supplied RC4 state buffer.
 * The input and output buffers may be the same buffer.
 * Since RC4 is a stream cypher, this function is used
 * for both encryption and decryption.
 */
void
rc4_crypt_skip(struct rc4_state *const state,
	const u_char *inbuf, u_char *outbuf, int buflen, int skip)
{
	int i;
	u_char j;

	for (i = 0; i < skip; i++) {

		/* Update modification indicies */
		state->index1++;
		state->index2 += state->perm[state->index1];

		/* Modify permutation */
		swap_bytes(&state->perm[state->index1],
		    &state->perm[state->index2]);

		/* Encrypt/decrypt next byte */
		j = state->perm[state->index1] + state->perm[state->index2];
	}
	for (i = 0; i < buflen; i++) {

		/* Update modification indicies */
		state->index1++;
		state->index2 += state->perm[state->index1];

		/* Modify permutation */
		swap_bytes(&state->perm[state->index1],
		    &state->perm[state->index2]);

		/* Encrypt/decrypt next byte */
		j = state->perm[state->index1] + state->perm[state->index2];
		outbuf[i] = inbuf[i] ^ state->perm[j];
	}
}
EXPORT_SYMBOL(rc4_crypt_skip);

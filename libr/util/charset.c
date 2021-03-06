/* radare - LGPL - Copyright 2020 - gogo, pancake */

#include <r_util.h>

#define USE_RUNES 0

R_API RCharset *r_charset_new(void) {
	RCharset* c = R_NEW0 (RCharset);
	if (!c) {
		return NULL;
	}
	c->db = NULL; // must be set after calling new by the caller
	return c;
}

R_API void r_charset_free(RCharset *c) {
	sdb_free (c->db);
	free (c);
}

R_API bool r_charset_open(RCharset *c, const char *cs) {
	r_return_val_if_fail (c && cs, false);
	sdb_reset (c->db);
	sdb_open (c->db, cs);
	sdb_reset (c->db_char_to_hex);
	sdb_open (c->db_char_to_hex, cs);

	c->db_char_to_hex = sdb_new0 ();

	SdbListIter *iter;
	SdbKv *kv;
	SdbList *sdbls = sdb_foreach_list (c->db, true);

	ls_foreach (sdbls, iter, kv) {
		const char *new_key = kv->base.value;
		const char *new_value = kv->base.key;
		sdb_add (c->db_char_to_hex, new_key, new_value, 0);
	}
	ls_free (sdbls);

	return true;
}

// rune
R_API RCharsetRune *r_charset_rune_new(const ut8 *ch, const ut8 *hx) {
	RCharsetRune* c = R_NEW0 (RCharsetRune);
	if (!c) {
		return NULL;
	}
	c->ch = (ut8 *) strdup ((char *) ch);
	c->hx = (ut8 *) strdup ((char *) hx);
	c->left = NULL;
	c->right = NULL;
	return c;
}

R_API void r_charset_rune_free(RCharsetRune *c) {
	free (c->ch);
	free (c->hx);
	free (c);
}

R_API RCharsetRune *add_rune(RCharsetRune *r, const ut8 *ch, const ut8 *hx) {
	if (!r) {
		r = r_charset_rune_new (ch, hx);
	}
	int cmp = strcmp ((char *)hx, (char *)r->hx);
	if (cmp < 0) {
		r->left = add_rune (r->left, ch, hx);
	} else if (cmp > 0) {
		r->right = add_rune (r->right, ch, hx);
	} else {
		int cmp = strcmp ((char *)ch, (char *)r->ch);
		if (cmp > 0) {
			r->left = add_rune (r->left, ch, hx);
		} else if (cmp < 0) {
			r->right = add_rune (r->right, ch, hx);
		}
	}
	return r;
}

R_API RCharsetRune *search_from_hex(RCharsetRune *r, const ut8 *hx) {
	if (!r) {
		return NULL;
	}
	if (!strcmp ((char *)r->hx, (char *)hx)) {
		return r;
	}
	RCharsetRune *left = search_from_hex (r->left, hx);
	return left? left: search_from_hex (r->right, hx);
}

// assumes out is as big as in_len
R_API size_t r_charset_encode_str(RCharset *rc, ut8 *out, size_t out_len, const ut8 *in, size_t in_len) {
	char k[32];
	char *o = (char*)out;
	int i;
	for (i = 0; i < in_len; i++) {
		ut8 ch_in = in[i];

		snprintf (k, sizeof (k), "0x%02x", ch_in);
		const char *v = sdb_const_get (rc->db, k, 0);
		const char *ret = r_str_get_fail (v, "?");

		strcpy (o, ret);

		o += strlen (o);
	}

	return o - (char*)out;
}

// assumes out is as big as in_len
R_API size_t r_charset_decode_str(RCharset *rc, ut8 *out, size_t out_len, const ut8 *in, size_t in_len) {
	char k[32];
	char *o = (char*)out;
	int i;
	for (i = 0; i < in_len; i++) {
		ut8 ch_in = in[i];

		//zero terminate the string
		snprintf (k, sizeof (k), "%c", ch_in);//snprintf (k, sizeof (k), "0x%02x", ch_in);
		char *v = sdb_get (rc->db_char_to_hex, k, 0);
		memmove(v, v+2, strlen (v));

		//convert to ascii
		char str_hx[32];
		snprintf (str_hx, sizeof (str_hx), "%c", (char) strtol( v, 0, 16));
		const char *ret = r_str_get_fail (str_hx, "?");
		strcpy (o, ret);

		o += strlen (o);
	}
	return o - (char*)out;
}

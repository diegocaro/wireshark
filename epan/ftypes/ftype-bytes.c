/*
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2001 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <ftypes-int.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <epan/addr_resolv.h>
#include <epan/strutil.h>
#include <epan/oids.h>
#include <epan/osi-utils.h>
#include <epan/to_str.h>

static void
bytes_fvalue_new(fvalue_t *fv)
{
	fv->value.bytes = NULL;
}

static void
bytes_fvalue_copy(fvalue_t *dst, const fvalue_t *src)
{
	dst->value.bytes = g_bytes_ref(src->value.bytes);
}

static void
bytes_fvalue_free(fvalue_t *fv)
{
	if (fv->value.bytes) {
		g_bytes_unref(fv->value.bytes);
		fv->value.bytes = NULL;
	}
}


static void
bytes_fvalue_set(fvalue_t *fv, GBytes *value)
{
	/* Free up the old value, if we have one */
	bytes_fvalue_free(fv);

	fv->value.bytes = g_bytes_ref(value);
}

static GBytes *
bytes_fvalue_get(fvalue_t *fv)
{
	return g_bytes_ref(fv->value.bytes);
}

static char *
oid_to_repr(wmem_allocator_t *scope, const fvalue_t *fv, ftrepr_t rtype _U_, int field_display _U_)
{
	return oid_encoded2string(scope, g_bytes_get_data(fv->value.bytes, NULL), (guint)g_bytes_get_size(fv->value.bytes));
}

static char *
rel_oid_to_repr(wmem_allocator_t *scope, const fvalue_t *fv, ftrepr_t rtype _U_, int field_display _U_)
{
	return rel_oid_encoded2string(scope,  g_bytes_get_data(fv->value.bytes, NULL), (guint)g_bytes_get_size(fv->value.bytes));
}

static char *
system_id_to_repr(wmem_allocator_t *scope, const fvalue_t *fv, ftrepr_t rtype _U_, int field_display _U_)
{
	return print_system_id(scope,  g_bytes_get_data(fv->value.bytes, NULL), (guint)g_bytes_get_size(fv->value.bytes));
}

char *
bytes_to_dfilter_repr(wmem_allocator_t *scope,
			const guint8 *src, size_t src_size)
{
	char *buf;
	size_t max_char_size;
	char *buf_ptr;

	/* Include space for extra punct and '\0'. */
	max_char_size = src_size * 3 + 1;

	buf = wmem_alloc(scope, max_char_size);
	buf_ptr = bytes_to_hexstr_punct(buf, src, src_size, ':');
	if (src_size == 1)
		*buf_ptr++ = ':';
	*buf_ptr = '\0';
	return buf;
}

static char *
bytes_to_repr(wmem_allocator_t *scope, const fvalue_t *fv, ftrepr_t rtype, int field_display)
{
	char separator;
	const uint8_t *bytes;
	gsize bytes_size;

	bytes = g_bytes_get_data(fv->value.bytes, &bytes_size);

	if (rtype == FTREPR_DFILTER) {
		if (bytes_size == 0) {
			/* An empty byte array in a display filter is represented as "" */
			return wmem_strdup(scope, "\"\"");
		}
		return bytes_to_dfilter_repr(scope, bytes, bytes_size);
	}

	switch(FIELD_DISPLAY(field_display))
	{
	case SEP_DOT:
		separator = '.';
		break;
	case SEP_DASH:
		separator = '-';
		break;
	case SEP_SPACE:
	case SEP_COLON:
	case BASE_NONE:
	default:
		separator = ':';
		break;
	}

	if (bytes_size) {
		return bytes_to_str_punct_maxlen(scope, bytes, bytes_size, separator, 0);
	}

	return wmem_strdup(scope, "");
}

static gboolean
bytes_from_string(fvalue_t *fv, const char *s, size_t len, gchar **err_msg _U_)
{
	GByteArray	*bytes;

	bytes = g_byte_array_new();

	if (len == 0)
		len = strlen(s);

	g_byte_array_append(bytes, (const guint8 *)s, (guint)len);

	/* Free up the old value, if we have one */
	bytes_fvalue_free(fv);
	fv->value.bytes = g_byte_array_free_to_bytes(bytes);

	return TRUE;
}

GByteArray *
byte_array_from_literal(const char *s, gchar **err_msg)
{
	GByteArray	*bytes;
	gboolean	res;

	/* Skip leading colon if any. */
	if (*s == ':')
		s++;

	/*
	 * Special case where the byte string is specified using a one byte
	 * hex literal. We can't allow this for byte strings that are longer
	 * than one byte, because then we'd have to know which endianness the
	 * byte string should be in.
	 */
	if (strlen(s) == 4 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
		s = s + 2;

	bytes = g_byte_array_new();

	/* Hack: If we have a binary number 0bXXXXXXXX use that as a byte array
	 * of length one. This is ambiguous because it can also be
	 * parsed (without separators) as a byte array of length 5:
	 * 	0bXXXXXXXX = 0b:XX:XX:XX:XX = { 0x0b, 0xXX, 0xXX, 0xXX, 0xXX } */
	if (strlen(s) == 10 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B') &&
						(s[2] == '0' || s[2] == '1')) {
		errno = 0;
		char *endptr;
		long number = strtol(s + 2, &endptr, 2);
		if (errno == 0 && *endptr == '\0' && number >= 0x0 && number <= 0xff) {
			guint8 byte = (guint8)number;
			g_byte_array_append(bytes, &byte, 1);
			return bytes;
		}
	}

	res = hex_str_to_bytes(s, bytes, FALSE);

	if (!res) {
		if (err_msg != NULL)
			*err_msg = ws_strdup_printf("\"%s\" is not a valid byte string.", s);
		g_byte_array_free(bytes, TRUE);
		return NULL;
	}

	return bytes;
}

static gboolean
bytes_from_literal(fvalue_t *fv, const char *s, gboolean allow_partial_value _U_, gchar **err_msg)
{
	GByteArray	*bytes;

	bytes = byte_array_from_literal(s, err_msg);
	if (bytes == NULL)
		return FALSE;

	/* Free up the old value, if we have one */
	bytes_fvalue_free(fv);

	fv->value.bytes = g_byte_array_free_to_bytes(bytes);

	return TRUE;
}

GByteArray *
byte_array_from_charconst(unsigned long num, gchar **err_msg)
{
	if (num > UINT8_MAX) {
		if (err_msg) {
			*err_msg = ws_strdup_printf("%lu is too large for a byte value", num);
		}
		return NULL;
	}

	GByteArray *bytes = g_byte_array_new();
	uint8_t one_byte = (uint8_t)num;
	g_byte_array_append(bytes, &one_byte, 1);
	return bytes;
}

static gboolean
bytes_from_charconst(fvalue_t *fv, unsigned long num, gchar **err_msg)
{
	GByteArray	*bytes;

	bytes = byte_array_from_charconst(num, err_msg);
	if (bytes == NULL)
		return FALSE;

	/* Free up the old value, if we have one */
	bytes_fvalue_free(fv);

	fv->value.bytes = g_byte_array_free_to_bytes(bytes);

	return TRUE;
}

static gboolean
ax25_from_literal(fvalue_t *fv, const char *s, gboolean allow_partial_value, gchar **err_msg)
{
	/*
	 * Don't request an error message if bytes_from_literal fails;
	 * if it does, we'll report an error specific to this address
	 * type.
	 */
	if (bytes_from_literal(fv, s, TRUE, NULL)) {
		if (g_bytes_get_size(fv->value.bytes) > FT_AX25_ADDR_LEN) {
			if (err_msg != NULL) {
				*err_msg = ws_strdup_printf("\"%s\" contains too many bytes to be a valid AX.25 address.",
				    s);
			}
			return FALSE;
		}
		else if (g_bytes_get_size(fv->value.bytes) < FT_AX25_ADDR_LEN && !allow_partial_value) {
			if (err_msg != NULL) {
				*err_msg = ws_strdup_printf("\"%s\" contains too few bytes to be a valid AX.25 address.",
				    s);
			}
			return FALSE;
		}

		return TRUE;
	}

	/*
	 * XXX - what needs to be done here is something such as:
	 *
	 * Look for a "-" in the string.
	 *
	 * If we find it, make sure that there are 1-6 alphanumeric
	 * ASCII characters before it, and that there are 2 decimal
	 * digits after it, from 00 to 15; if we don't find it, make
	 * sure that there are 1-6 alphanumeric ASCII characters
	 * in the string.
	 *
	 * If so, make the first 6 octets of the address the ASCII
	 * characters, with lower-case letters mapped to upper-case
	 * letters, shifted left by 1 bit, padded to 6 octets with
	 * spaces, also shifted left by 1 bit, and, if we found a
	 * "-", convert what's after it to a number and make the 7th
	 * octet the number, shifted left by 1 bit, otherwise make the
	 * 7th octet zero.
	 *
	 * We should also change all the comparison functions for
	 * AX.25 addresses check the upper 7 bits of all but the last
	 * octet of the address, ignoring the "end of address" bit,
	 * and compare only the 4 bits above the low-order bit for
	 * the last octet, ignoring the "end of address" bit and
	 * various reserved bits and bits used for other purposes.
	 *
	 * See section 3.12 "Address-Field Encoding" of the AX.25
	 * spec and
	 *
	 *	http://www.itu.int/ITU-R/terrestrial/docs/fixedmobile/fxm-art19-sec3.pdf
	 */
	if (err_msg != NULL)
		*err_msg = ws_strdup_printf("\"%s\" is not a valid AX.25 address.", s);
	return FALSE;
}

static gboolean
vines_from_literal(fvalue_t *fv, const char *s, gboolean allow_partial_value, gchar **err_msg)
{
	/*
	 * Don't request an error message if bytes_from_literal fails;
	 * if it does, we'll report an error specific to this address
	 * type.
	 */
	if (bytes_from_literal(fv, s, TRUE, NULL)) {
		if (g_bytes_get_size(fv->value.bytes) > FT_VINES_ADDR_LEN) {
			if (err_msg != NULL) {
				*err_msg = ws_strdup_printf("\"%s\" contains too many bytes to be a valid Vines address.",
				    s);
			}
			return FALSE;
		}
		else if (g_bytes_get_size(fv->value.bytes) < FT_VINES_ADDR_LEN && !allow_partial_value) {
			if (err_msg != NULL) {
				*err_msg = ws_strdup_printf("\"%s\" contains too few bytes to be a valid Vines address.",
				    s);
			}
			return FALSE;
		}

		return TRUE;
	}

	/* XXX - need better validation of Vines address */

	if (err_msg != NULL)
		*err_msg = ws_strdup_printf("\"%s\" is not a valid Vines address.", s);
	return FALSE;
}

static gboolean
ether_from_literal(fvalue_t *fv, const char *s, gboolean allow_partial_value, gchar **err_msg)
{
	/*
	 * Don't request an error message if bytes_from_literal fails;
	 * if it does, we'll report an error specific to this address
	 * type.
	 */
	if (bytes_from_literal(fv, s, TRUE, NULL)) {
		if (g_bytes_get_size(fv->value.bytes) > FT_ETHER_LEN) {
			if (err_msg != NULL) {
				*err_msg = ws_strdup_printf("\"%s\" contains too many bytes to be a valid Ethernet address.",
				    s);
			}
			return FALSE;
		}
		else if (g_bytes_get_size(fv->value.bytes) < FT_ETHER_LEN && !allow_partial_value) {
			if (err_msg != NULL) {
				*err_msg = ws_strdup_printf("\"%s\" contains too few bytes to be a valid Ethernet address.",
				    s);
			}
			return FALSE;
		}

		return TRUE;
	}

	/* XXX - Try resolving as an Ethernet host name and parse that? */

	if (err_msg != NULL)
		*err_msg = ws_strdup_printf("\"%s\" is not a valid Ethernet address.", s);
	return FALSE;
}

static gboolean
oid_from_literal(fvalue_t *fv, const char *s, gboolean allow_partial_value _U_, gchar **err_msg)
{
	GByteArray	*bytes;
	gboolean	res;


#if 0
	/*
	 * Don't log a message if this fails; we'll try looking it
	 * up as an OID if it does, and if that fails,
	 * we'll log a message.
	 */
	/* do not try it as '.' is handled as valid separator for hexbytes :( */
	if (bytes_from_literal(fv, s, TRUE, NULL)) {
		return TRUE;
	}
#endif

	bytes = g_byte_array_new();
	res = oid_str_to_bytes(s, bytes);
	if (!res) {
		if (err_msg != NULL)
			*err_msg = ws_strdup_printf("\"%s\" is not a valid OBJECT IDENTIFIER.", s);
		g_byte_array_free(bytes, TRUE);
		return FALSE;
	}

	/* Free up the old value, if we have one */
	bytes_fvalue_free(fv);
	fv->value.bytes = g_byte_array_free_to_bytes(bytes);

	return TRUE;
}

static gboolean
rel_oid_from_literal(fvalue_t *fv, const char *s, gboolean allow_partial_value _U_, gchar **err_msg)
{
	GByteArray	*bytes;
	gboolean	res;

	bytes = g_byte_array_new();
	res = rel_oid_str_to_bytes(s, bytes, FALSE);
	if (!res) {
		if (err_msg != NULL)
			*err_msg = ws_strdup_printf("\"%s\" is not a valid RELATIVE-OID.", s);
		g_byte_array_free(bytes, TRUE);
		return FALSE;
	}

	/* Free up the old value, if we have one */
	bytes_fvalue_free(fv);
	fv->value.bytes = g_byte_array_free_to_bytes(bytes);

	return TRUE;
}

static gboolean
system_id_from_literal(fvalue_t *fv, const char *s, gboolean allow_partial_value _U_, gchar **err_msg)
{
	/*
	 * Don't request an error message if bytes_from_literal fails;
	 * if it does, we'll report an error specific to this address
	 * type.
	 */
	if (bytes_from_literal(fv, s, TRUE, NULL)) {
		if (g_bytes_get_size(fv->value.bytes) > MAX_SYSTEMID_LEN) {
			if (err_msg != NULL) {
				*err_msg = ws_strdup_printf("\"%s\" contains too many bytes to be a valid OSI System-ID.",
				    s);
			}
			return FALSE;
		}

		return TRUE;
	}

	/* XXX - need better validation of OSI System-ID address */

	if (err_msg != NULL)
		*err_msg = ws_strdup_printf("\"%s\" is not a valid OSI System-ID.", s);
	return FALSE;
}

static gboolean
fcwwn_from_literal(fvalue_t *fv, const char *s, gboolean allow_partial_value _U_, gchar **err_msg)
{
	/*
	 * Don't request an error message if bytes_from_literal fails;
	 * if it does, we'll report an error specific to this address
	 * type.
	 */
	if (bytes_from_literal(fv, s, TRUE, NULL)) {
		if (g_bytes_get_size(fv->value.bytes) > FT_FCWWN_LEN) {
			if (err_msg != NULL) {
				*err_msg = ws_strdup_printf("\"%s\" contains too many bytes to be a valid FCWWN.",
				    s);
			}
			return FALSE;
		}

		return TRUE;
	}

	/* XXX - need better validation of FCWWN address */

	if (err_msg != NULL)
		*err_msg = ws_strdup_printf("\"%s\" is not a valid FCWWN.", s);
	return FALSE;
}

static guint
len(fvalue_t *fv)
{
	return (guint)g_bytes_get_size(fv->value.bytes);
}

static void
slice(fvalue_t *fv, GByteArray *bytes, guint offset, guint length)
{
	guint8* data;

	data = (guint8 *)g_bytes_get_data(fv->value.bytes, NULL) + offset;

	g_byte_array_append(bytes, data, length);
}

static enum ft_result
cmp_order(const fvalue_t *fv_a, const fvalue_t *fv_b, int *cmp)
{
	*cmp = g_bytes_compare(fv_a->value.bytes, fv_b->value.bytes);
	return FT_OK;
}

static enum ft_result
bytes_bitwise_and(fvalue_t *fv_dst, const fvalue_t *fv_a, const fvalue_t *fv_b, char **err_ptr _U_)
{
	GByteArray	*dst;
	const guint8 *p_a, *p_b;
	gsize size_a, size_b;

	p_a = g_bytes_get_data(fv_a->value.bytes, &size_a);
	p_b = g_bytes_get_data(fv_b->value.bytes, &size_b);

	gsize len = MIN(size_a, size_b);
	if (len == 0) {
		fv_dst->value.bytes = g_bytes_new(NULL, 0);
		return FT_OK;
	}

	dst = g_byte_array_sized_new((guint)len);
	for (gsize i = 0; i < len; i++) {
		guint8 byte = p_a[i] & p_b[i];
		g_byte_array_append(dst, &byte, 1);
	}
	fv_dst->value.bytes = g_byte_array_free_to_bytes(dst);
	return FT_OK;
}

static enum ft_result
cmp_contains(const fvalue_t *fv_a, const fvalue_t *fv_b, gboolean *contains)
{
	const void *data_a, *data_b;
	gsize size_a, size_b;

	data_a = g_bytes_get_data(fv_a->value.bytes, &size_a);
	data_b = g_bytes_get_data(fv_b->value.bytes, &size_b);

	if (ws_memmem(data_a, size_a, data_b, size_b)) {
		*contains = TRUE;
	}
	else {
		*contains = FALSE;
	}

	return FT_OK;
}

static enum ft_result
cmp_matches(const fvalue_t *fv, const ws_regex_t *regex, gboolean *matches)
{
	const void *data;
	gsize data_size;

	data = g_bytes_get_data(fv->value.bytes, &data_size);

	*matches = ws_regex_matches_length(regex, data, data_size);
	return FT_OK;
}

static gboolean
bytes_is_zero(const fvalue_t *fv)
{
	const uint8_t *data;
	gsize data_size;

	data = g_bytes_get_data(fv->value.bytes, &data_size);

	if (data_size == 0)
		return TRUE;

	for (gsize i = 0; i < data_size; i++) {
		if (data[i] != 0) {
			return FALSE;
		}
	}
	return TRUE;
}

void
ftype_register_bytes(void)
{

	static ftype_t bytes_type = {
		FT_BYTES,			/* ftype */
		"FT_BYTES",			/* name */
		"Byte sequence",		/* pretty_name */
		0,				/* wire_size */
		bytes_fvalue_new,		/* new_value */
		bytes_fvalue_copy,		/* copy_value */
		bytes_fvalue_free,		/* free_value */
		bytes_from_literal,		/* val_from_literal */
		bytes_from_string,		/* val_from_string */
		bytes_from_charconst,		/* val_from_charconst */
		bytes_to_repr,			/* val_to_string_repr */

		NULL,				/* val_to_uinteger64 */
		NULL,				/* val_to_sinteger64 */

		{ .set_value_bytes = bytes_fvalue_set },	/* union set_value */
		{ .get_value_bytes = bytes_fvalue_get },	/* union get_value */

		cmp_order,
		cmp_contains,
		cmp_matches,

		bytes_is_zero,			/* is_zero */
		NULL,				/* is_negative */
		len,
		slice,
		bytes_bitwise_and,		/* bitwise_and */
		NULL,				/* unary_minus */
		NULL,				/* add */
		NULL,				/* subtract */
		NULL,				/* multiply */
		NULL,				/* divide */
		NULL,				/* modulo */
	};

	static ftype_t uint_bytes_type = {
		FT_UINT_BYTES,		/* ftype */
		"FT_UINT_BYTES",		/* name */
		"Byte sequence",		/* pretty_name */
		0,				/* wire_size */
		bytes_fvalue_new,		/* new_value */
		bytes_fvalue_copy,		/* copy_value */
		bytes_fvalue_free,		/* free_value */
		bytes_from_literal,		/* val_from_literal */
		NULL,				/* val_from_string */
		NULL,				/* val_from_charconst */
		bytes_to_repr,			/* val_to_string_repr */

		NULL,				/* val_to_uinteger64 */
		NULL,				/* val_to_sinteger64 */

		{ .set_value_bytes = bytes_fvalue_set },	/* union set_value */
		{ .get_value_bytes = bytes_fvalue_get },	/* union get_value */

		cmp_order,
		cmp_contains,
		NULL,				/* cmp_matches */

		bytes_is_zero,			/* is_zero */
		NULL,				/* is_negative */
		len,
		slice,
		bytes_bitwise_and,		/* bitwise_and */
		NULL,				/* unary_minus */
		NULL,				/* add */
		NULL,				/* subtract */
		NULL,				/* multiply */
		NULL,				/* divide */
		NULL,				/* modulo */
	};

	static ftype_t ax25_type = {
		FT_AX25,			/* ftype */
		"FT_AX25",			/* name */
		"AX.25 address",		/* pretty_name */
		FT_AX25_ADDR_LEN,		/* wire_size */
		bytes_fvalue_new,		/* new_value */
		bytes_fvalue_copy,		/* copy_value */
		bytes_fvalue_free,		/* free_value */
		ax25_from_literal,		/* val_from_literal */
		NULL,				/* val_from_string */
		NULL,				/* val_from_charconst */
		bytes_to_repr,			/* val_to_string_repr */

		NULL,				/* val_to_uinteger64 */
		NULL,				/* val_to_sinteger64 */

		{ .set_value_bytes = bytes_fvalue_set },	/* union set_value */
		{ .get_value_bytes = bytes_fvalue_get },	/* union get_value */

		cmp_order,
		cmp_contains,
		cmp_matches,

		bytes_is_zero,			/* is_zero */
		NULL,				/* is_negative */
		len,
		slice,
		bytes_bitwise_and,		/* bitwise_and */
		NULL,				/* unary_minus */
		NULL,				/* add */
		NULL,				/* subtract */
		NULL,				/* multiply */
		NULL,				/* divide */
		NULL,				/* modulo */
	};

	static ftype_t vines_type = {
		FT_VINES,			/* ftype */
		"FT_VINES",			/* name */
		"VINES address",		/* pretty_name */
		FT_VINES_ADDR_LEN,		/* wire_size */
		bytes_fvalue_new,		/* new_value */
		bytes_fvalue_copy,		/* copy_value */
		bytes_fvalue_free,		/* free_value */
		vines_from_literal,		/* val_from_literal */
		NULL,				/* val_from_string */
		NULL,				/* val_from_charconst */
		bytes_to_repr,			/* val_to_string_repr */

		NULL,				/* val_to_uinteger64 */
		NULL,				/* val_to_sinteger64 */

		{ .set_value_bytes = bytes_fvalue_set },	/* union set_value */
		{ .get_value_bytes = bytes_fvalue_get },	/* union get_value */

		cmp_order,
		cmp_contains,
		cmp_matches,

		bytes_is_zero,			/* is_zero */
		NULL,				/* is_negative */
		len,
		slice,
		bytes_bitwise_and,		/* bitwise_and */
		NULL,				/* unary_minus */
		NULL,				/* add */
		NULL,				/* subtract */
		NULL,				/* multiply */
		NULL,				/* divide */
		NULL,				/* modulo */
	};

	static ftype_t ether_type = {
		FT_ETHER,			/* ftype */
		"FT_ETHER",			/* name */
		"Ethernet or other MAC address",/* pretty_name */
		FT_ETHER_LEN,			/* wire_size */
		bytes_fvalue_new,		/* new_value */
		bytes_fvalue_copy,		/* copy_value */
		bytes_fvalue_free,		/* free_value */
		ether_from_literal,		/* val_from_literal */
		NULL,				/* val_from_string */
		NULL,				/* val_from_charconst */
		bytes_to_repr,			/* val_to_string_repr */

		NULL,				/* val_to_uinteger64 */
		NULL,				/* val_to_sinteger64 */

		{ .set_value_bytes = bytes_fvalue_set },	/* union set_value */
		{ .get_value_bytes = bytes_fvalue_get },	/* union get_value */

		cmp_order,
		cmp_contains,
		cmp_matches,

		bytes_is_zero,			/* is_zero */
		NULL,				/* is_negative */
		len,
		slice,
		bytes_bitwise_and,		/* bitwise_and */
		NULL,				/* unary_minus */
		NULL,				/* add */
		NULL,				/* subtract */
		NULL,				/* multiply */
		NULL,				/* divide */
		NULL,				/* modulo */
	};

	static ftype_t oid_type = {
		FT_OID,			/* ftype */
		"FT_OID",			/* name */
		"ASN.1 object identifier",	/* pretty_name */
		0,			/* wire_size */
		bytes_fvalue_new,		/* new_value */
		bytes_fvalue_copy,		/* copy_value */
		bytes_fvalue_free,		/* free_value */
		oid_from_literal,		/* val_from_literal */
		NULL,				/* val_from_string */
		NULL,				/* val_from_charconst */
		oid_to_repr,			/* val_to_string_repr */

		NULL,				/* val_to_uinteger64 */
		NULL,				/* val_to_sinteger64 */

		{ .set_value_bytes = bytes_fvalue_set },	/* union set_value */
		{ .get_value_bytes = bytes_fvalue_get },	/* union get_value */

		cmp_order,
		cmp_contains,
		NULL,				/* cmp_matches */

		bytes_is_zero,			/* is_zero */
		NULL,				/* is_negative */
		len,
		slice,
		bytes_bitwise_and,		/* bitwise_and */
		NULL,				/* unary_minus */
		NULL,				/* add */
		NULL,				/* subtract */
		NULL,				/* multiply */
		NULL,				/* divide */
		NULL,				/* modulo */
	};

	static ftype_t rel_oid_type = {
		FT_REL_OID,			/* ftype */
		"FT_REL_OID",			/* name */
		"ASN.1 relative object identifier",	/* pretty_name */
		0,			/* wire_size */
		bytes_fvalue_new,		/* new_value */
		bytes_fvalue_copy,		/* copy_value */
		bytes_fvalue_free,		/* free_value */
		rel_oid_from_literal,		/* val_from_literal */
		NULL,				/* val_from_string */
		NULL,				/* val_from_charconst */
		rel_oid_to_repr,		/* val_to_string_repr */

		NULL,				/* val_to_uinteger64 */
		NULL,				/* val_to_sinteger64 */

		{ .set_value_bytes = bytes_fvalue_set },	/* union set_value */
		{ .get_value_bytes = bytes_fvalue_get },	/* union get_value */

		cmp_order,
		cmp_contains,
		NULL,				/* cmp_matches */

		bytes_is_zero,			/* is_zero */
		NULL,				/* is_negative */
		len,
		slice,
		bytes_bitwise_and,		/* bitwise_and */
		NULL,				/* unary_minus */
		NULL,				/* add */
		NULL,				/* subtract */
		NULL,				/* multiply */
		NULL,				/* divide */
		NULL,				/* modulo */
	};

	static ftype_t system_id_type = {
		FT_SYSTEM_ID,			/* ftype */
		"FT_SYSTEM_ID",			/* name */
		"OSI System-ID",		/* pretty_name */
		0,			/* wire_size */
		bytes_fvalue_new,		/* new_value */
		bytes_fvalue_copy,		/* copy_value */
		bytes_fvalue_free,		/* free_value */
		system_id_from_literal,		/* val_from_literal */
		NULL,				/* val_from_string */
		NULL,				/* val_from_charconst */
		system_id_to_repr,		/* val_to_string_repr */

		NULL,				/* val_to_uinteger64 */
		NULL,				/* val_to_sinteger64 */

		{ .set_value_bytes = bytes_fvalue_set }, /* union set_value */
		{ .get_value_bytes = bytes_fvalue_get },	/* union get_value */

		cmp_order,
		cmp_contains,
		NULL,				/* cmp_matches */

		bytes_is_zero,			/* is_zero */
		NULL,				/* is_negative */
		len,
		slice,
		bytes_bitwise_and,		/* bitwise_and */
		NULL,				/* unary_minus */
		NULL,				/* add */
		NULL,				/* subtract */
		NULL,				/* multiply */
		NULL,				/* divide */
		NULL,				/* modulo */
	};

	static ftype_t fcwwn_type = {
		FT_FCWWN,			/* ftype */
		"FT_FCWWN",			/* name */
		"Fibre Channel WWN",	/* pretty_name */
		FT_FCWWN_LEN,			/* wire_size */
		bytes_fvalue_new,		/* new_value */
		bytes_fvalue_copy,		/* copy_value */
		bytes_fvalue_free,		/* free_value */
		fcwwn_from_literal,		/* val_from_literal */
		NULL,				/* val_from_string */
		NULL,				/* val_from_charconst */
		bytes_to_repr,			/* val_to_string_repr */

		NULL,				/* val_to_uinteger64 */
		NULL,				/* val_to_sinteger64 */

		{ .set_value_bytes = bytes_fvalue_set },	/* union set_value */
		{ .get_value_bytes = bytes_fvalue_get },	/* union get_value */

		cmp_order,
		cmp_contains,
		cmp_matches,

		bytes_is_zero,			/* is_zero */
		NULL,				/* is_negative */
		len,
		slice,
		bytes_bitwise_and,		/* bitwise_and */
		NULL,				/* unary_minus */
		NULL,				/* add */
		NULL,				/* subtract */
		NULL,				/* multiply */
		NULL,				/* divide */
		NULL,				/* modulo */
	};

	ftype_register(FT_BYTES, &bytes_type);
	ftype_register(FT_UINT_BYTES, &uint_bytes_type);
	ftype_register(FT_AX25, &ax25_type);
	ftype_register(FT_VINES, &vines_type);
	ftype_register(FT_ETHER, &ether_type);
	ftype_register(FT_OID, &oid_type);
	ftype_register(FT_REL_OID, &rel_oid_type);
	ftype_register(FT_SYSTEM_ID, &system_id_type);
	ftype_register(FT_FCWWN, &fcwwn_type);
}

void
ftype_register_pseudofields_bytes(int proto)
{
	static int hf_ft_bytes;
	static int hf_ft_uint_bytes;
	static int hf_ft_ax25;
	static int hf_ft_vines;
	static int hf_ft_ether;
	static int hf_ft_oid;
	static int hf_ft_rel_oid;
	static int hf_ft_system_id;

	static hf_register_info hf_ftypes[] = {
		{ &hf_ft_bytes,
		    { "FT_BYTES", "_ws.ftypes.bytes",
			FT_BYTES, BASE_NONE, NULL, 0x00,
			NULL, HFILL }
		},
		{ &hf_ft_uint_bytes,
		    { "FT_UINT_BYTES", "_ws.ftypes.uint_bytes",
			FT_UINT_BYTES, BASE_NONE, NULL, 0x00,
			NULL, HFILL }
		},
		{ &hf_ft_ax25,
		    { "FT_AX25", "_ws.ftypes.ax25",
			FT_AX25, BASE_NONE, NULL, 0x00,
			NULL, HFILL }
		},
		{ &hf_ft_vines,
		    { "FT_VINES", "_ws.ftypes.vines",
			FT_VINES, BASE_NONE, NULL, 0x00,
			NULL, HFILL }
		},
		{ &hf_ft_ether,
		    { "FT_ETHER", "_ws.ftypes.ether",
			FT_ETHER, BASE_NONE, NULL, 0x00,
			NULL, HFILL }
		},
		{ &hf_ft_oid,
		    { "FT_OID", "_ws.ftypes.oid",
			FT_OID, BASE_NONE, NULL, 0x00,
			NULL, HFILL }
		},
		{ &hf_ft_rel_oid,
		    { "FT_REL_OID", "_ws.ftypes.rel_oid",
			FT_REL_OID, BASE_NONE, NULL, 0x00,
			NULL, HFILL }
		},
		{ &hf_ft_system_id,
		    { "FT_SYSTEM_ID", "_ws.ftypes.system_id",
			FT_SYSTEM_ID, BASE_NONE, NULL, 0x00,
			NULL, HFILL }
		},
	};

	proto_register_field_array(proto, hf_ftypes, array_length(hf_ftypes));
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 8
 * tab-width: 8
 * indent-tabs-mode: t
 * End:
 *
 * vi: set shiftwidth=8 tabstop=8 noexpandtab:
 * :indentSize=8:tabSize=8:noTabs=false:
 */

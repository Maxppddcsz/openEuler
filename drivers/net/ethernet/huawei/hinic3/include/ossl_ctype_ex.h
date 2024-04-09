/*
 * Huawei HiNIC PCI Express Linux driver
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

#ifndef OSSL_CTYPE_EX_H
#define OSSL_CTYPE_EX_H

#include "ossl_types.h"

#define BASE_ALL 0
#define BASE_8 8
#define BASE_10 10
#define BASE_16 16

#define S32_MAX 2147483647
#define S32_MIN (-2147483648)

#define S16_MAX 32767
#define S16_MIN (-32768)

#define S8_MAX 127
#define S8_MIN (-128)

#define U32_MAX 0xFFFFFFFF
#define U16_MAX 0xFFFF
#define U8_MAX 0xFF

#define uda_str2l strtol

/* *
 * string_tol - string to s64
 * @nptr: the string
 * @value: the output value
*  */
static __inline__ uda_status string_tol(const char *nptr, int base, s64 *value)
{
	char *endptr = NULL;
	s64 tmp_value;

	tmp_value = uda_str2l(nptr, &endptr, base);
	if (*endptr != 0)
		return -UDA_EINVAL;

	*value = tmp_value;
	return UDA_SUCCESS;
}

/* *
 * string_tol - string to u64
 * @nptr: the string
 * @value: the output value
*  */
static __inline__ uda_status string_toul(const char *nptr, int base, u64 *value)
{
	s64 tmp_value;

	if ((string_tol(nptr, base, &tmp_value) != UDA_SUCCESS) || tmp_value < 0) {
		return -UDA_EINVAL;
	}

	*value = (u64)tmp_value;
	return UDA_SUCCESS;
}

/* *
 * string_tol - string to s32
 * @nptr: the string
 * @value: the output value
*  */
static __inline__ uda_status string_toi(const char *nptr, int base, s32 *value)
{
	char *endptr = NULL;
	s64 tmp_value;

	tmp_value = strtol(nptr, &endptr, base);
	if ((*endptr != 0) || (tmp_value >= 0 && tmp_value > S32_MAX) ||
		(tmp_value < 0 && (tmp_value < S32_MIN))) {
		return -UDA_EINVAL;
	}
	*value = (s32)tmp_value;
	return UDA_SUCCESS;
}

/* *
 * string_tol - string to u32
 * @nptr: the string
 * @value: the output value
*  */
static __inline__ uda_status string_toui(const char *nptr, int base, u32 *value)
{
	char *endptr = NULL;
	s64 tmp_value;

	tmp_value = strtol(nptr, &endptr, base);
	if ((*endptr != 0) || tmp_value > U32_MAX || tmp_value < 0) {
		return -UDA_EINVAL;
	}
	*value = (u32)tmp_value;
	return UDA_SUCCESS;
}

/* *
 * string_tol - string to s16
 * @nptr: the string
 * @value: the output value
*  */
static __inline__ uda_status string_tos(const char *nptr, int base, s16 *value)
{
	char *endptr = NULL;
	s64 tmp_value;

	tmp_value = strtol(nptr, &endptr, base);
	if ((*endptr != 0) || (tmp_value > S16_MAX) || (tmp_value < S16_MIN)) {
		return -UDA_EINVAL;
	}
	*value = (s16)tmp_value;
	return UDA_SUCCESS;
}

/* *
 * string_tol - string to u16
 * @nptr: the string
 * @value: the output value
*  */
static __inline__ uda_status string_tous(const char *nptr, int base, u16 *value)
{
	char *endptr = NULL;
	s64 tmp_value;

	tmp_value = strtol(nptr, &endptr, base);
	if ((*endptr != 0) || tmp_value > U16_MAX || tmp_value < 0) {
		return -UDA_EINVAL;
	}
	*value = (u16)tmp_value;
	return UDA_SUCCESS;
}

/* *
 * string_tol - string to s8
 * @nptr: the string
 * @value: the output value
*  */
static __inline__ uda_status string_tob(const char *nptr, int base, s8 *value)
{
	char *endptr = NULL;
	s64 tmp_value;

	tmp_value = strtol(nptr, &endptr, base);
	if ((*endptr != 0) || tmp_value > S8_MAX || (tmp_value < S8_MIN)) {
		return -UDA_EINVAL;
	}
	*value = (s8)tmp_value;
	return UDA_SUCCESS;
}

/* *
 * string_tol - string to u8
 * @nptr: the string
 * @value: the output value
*  */
static __inline__ uda_status string_toub(const char *nptr, int base, u8 *value)
{
	char *endptr = NULL;
	s64 tmp_value;

	tmp_value = strtol(nptr, &endptr, base);
	if ((*endptr != 0) || tmp_value > U8_MAX || tmp_value < 0) {
		return -UDA_EINVAL;
	}
	*value = (u8)tmp_value;
	return UDA_SUCCESS;
}

/* *
 * char_check - first character of string check
 * @cmd: the input string
*  */
static __inline__ bool char_check(const char cmd)
{
	if (cmd >= 'a' && cmd <= 'z') {
		return UDA_TRUE;
	}

	if (cmd >= 'A' && cmd <= 'Z') {
		return UDA_TRUE;
	}

	return UDA_FALSE;
}

#endif /* OSSL_CTYPE_EX_H */

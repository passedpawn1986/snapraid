/*
 * Copyright (C) 2011 Andrea Mazzoleni
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "portable.h"

#include "util.h"

/****************************************************************************/
/* string */

int strgets(char* s, unsigned size, FILE* f)
{
	char* i = s;
	char* send = s + size;
	int c;

	while (1) {
#if HAVE_FGETC_UNLOCKED
		c = fgetc_unlocked(f);
#else
		c = fgetc(f);
#endif        
		if (c == EOF || c == '\n')
			break;

		*i++ = c;

		if (i == send) {
			return -1;
		}
	}

	if (c == EOF) {
		if (
#if HAVE_FERROR_UNLOCKED
			ferror_unlocked(f)
#else
			ferror(f)
#endif
		) {
			return -1;
		}
		if (i == s)
			return 0; /* return EOF only if nothing was read */
	}

	/* remove ending carrige return to support the Windows CR+LF format */
	if (i != s && i[-1] == '\r')
		--i;

	*i = 0;

	return i - s;
}

int stru32(const char* s, uint32_t* value)
{
	uint32_t v;
	
	if (!*s)
		return -1;

	v = 0;
	while (*s>='0' && *s<='9') {
		v *= 10;
		v += *s - '0';
		++s;
	}

	if (*s)
		return -1;

	*value = v;

	return 0;
}

int stru64(const char* s, uint64_t* value)
{
	uint64_t v;

	if (!*s)
		return -1;

	v = 0;
	while (*s>='0' && *s<='9') {
		v *= 10;
		v += *s - '0';
		++s;
	}

	if (*s)
		return -1;

	*value = v;

	return 0;
}

static char strhexset[16] = "0123456789abcdef";

void strenchex(char* str, const void* void_data, unsigned data_len)
{
	const unsigned char* data = void_data;
	unsigned i;

	for(i=0;i<data_len;++i) {
		unsigned char b = data[i];
		*str++ = strhexset[b >> 4];
		*str++ = strhexset[b & 0xF];
	}
}

char* strdechex(void* void_data, unsigned data_len, char* str)
{
	unsigned char* data = void_data;
	unsigned i;

	for(i=0;i<data_len;++i) {
		char c0;
		char c1;
		unsigned char b0;
		unsigned char b1;

		c0 = *str;
		if (c0 >= 'A' && c0 <= 'F')
			b0 = c0 - 'A' + 10;
		else if (c0 >= 'a' && c0 <= 'f')
			b0 = c0 - 'a' + 10;
		else if (c0 >= '0' && c0 <= '9')
			b0 = c0 - '0';
		else
			return str;
		++str;

		c1 = *str;
		if (c1 >= 'A' && c1 <= 'F')
			b1 = c1 - 'A' + 10;
		else if (c1 >= 'a' && c1 <= 'f')
			b1 = c1 - 'a' + 10;
		else if (c1 >= '0' && c1 <= '9')
			b1 = c1 - '0';
		else
			return str;
		++str;

		data[i] = (b0 << 4) | b1;
	}

	return 0;
}

/****************************************************************************/
/* path */

void pathcpy(char* str, size_t size, const char* src)
{
	size_t len = strlen(src);
	
	if (len + 1 >= size) {
		fprintf(stderr, "Path too long\n");
		exit(EXIT_FAILURE);
	}

	memcpy(str, src, len + 1);
}

void pathimport(char* str, size_t size, const char* src)
{
	pathcpy(str, size, src);

#ifdef _WIN32
	/* convert all Windows '\' to '/' */
	while (*str) {
		if (*str == '\\')
			*str = '/';
		++str;
	}
#endif
}

void pathprint(char* str, size_t size, const char* format, ...)
{
	size_t len;
	va_list ap;
	
	va_start(ap, format);
	len = vsnprintf(str, size, format, ap);
	va_end(ap);

	if (len >= size) {
		fprintf(stderr, "Path too long\n");
		exit(EXIT_FAILURE);
	}
}

void pathslash(char* str, size_t size)
{
	size_t len = strlen(str);

	if (len > 0 && str[len - 1] != '/') {
		if (len + 2 >= size) {
			fprintf(stderr, "Path too long\n");
			exit(EXIT_FAILURE);
		}

		str[len] = '/';
		str[len+1] = 0;
	}
}

/****************************************************************************/
/* mem */

void* malloc_nofail(size_t size)
{
	void* ptr = malloc(size);

	if (!ptr) {
		fprintf(stderr, "Low memory\n");
		exit(EXIT_FAILURE);
	}

	return ptr;
}

#define ALIGN 256

void* malloc_nofail_align(size_t size, void** freeptr)
{
	unsigned char* ptr = malloc_nofail(size + ALIGN);
	uintptr_t offset;

	*freeptr = ptr;

	offset = ((uintptr_t)ptr) % ALIGN;

	if (offset != 0) {
		ptr += ALIGN - offset;
	}

	return ptr;
}

#if HAVE_LIBCRYPTO
#include <openssl/md5.h>
#else
#include "md5.c"
#endif

#include "murmurhash3.c"

void memhash(unsigned kind, void* digest, const void* src, unsigned size)
{
	if (kind == HASH_MURMUR3) {
		MurmurHash3_x86_128(src, size, 0, digest);
		return;
	}

	if (kind == HASH_MD5) {
#if HAVE_LIBCRYPTO
		MD5((void*)src, size, digest);
#else
		struct md5_t md5;
		md5_init(&md5);
		md5_update(&md5, src, size);
		md5_final(&md5, digest);
#endif
		return;
	}
}


#include "buf.h"
#include "gc.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Weft_Buf *new_buf(size_t cap)
{
	Weft_Buf *buf = malloc(sizeof(Weft_Buf) + cap);
	if (!buf) {
		exit(gc_error());
	}

	buf->cap = cap;
	buf->at = 0;

	return buf;
}

size_t buf_get_cap(const Weft_Buf *buf)
{
	return buf->cap;
}

size_t buf_get_at(const Weft_Buf *buf)
{
	return buf->at;
}

void *buf_get_raw(Weft_Buf *buf)
{
	return buf->raw;
}

void buf_print(const Weft_Buf *buf, size_t size, void *print_fn)
{
	size_t len = buf->at / size;
	void (*fn)(void *) = print_fn;

	for (size_t i = 0; i < len; i += size) {
		void **ptr = (void **)(buf->raw + i);
		fn(*ptr);
		if (i < len - size) {
			printf(" ");
		}
	}
}

static size_t realloc_cap(Weft_Buf *buf, size_t requested_size)
{
	return 2 * (buf->at + requested_size);
}

static Weft_Buf *realloc_buf(Weft_Buf *buf, size_t cap)
{
	buf = realloc(buf, sizeof(Weft_Buf) + cap);
	if (!buf) {
		exit(gc_error());
	}
	buf->cap = cap;

	return buf;
}

static bool is_shrinkable(const Weft_Buf *buf)
{
	return buf->at < buf->cap / 4;
}

static Weft_Buf *shrink_buf(Weft_Buf *buf)
{
	return realloc_buf(buf, realloc_cap(buf, 0));
}

static void shrink_if_possible(Weft_Buf **buf_p, Weft_Buf *buf)
{
	if (is_shrinkable(buf)) {
		*buf_p = shrink_buf(buf);
	}
}

void buf_clear(Weft_Buf **buf_p)
{
	Weft_Buf *buf = *buf_p;
	buf->at = 0;
	shrink_if_possible(buf_p, buf);
}

void buf_push(Weft_Buf **buf_p, const void *src, size_t size)
{
	Weft_Buf *buf = *buf_p;
	if (buf->at + size > buf->cap) {
		buf = realloc_buf(buf, realloc_cap(buf, size));
		*buf_p = buf;
	}

	memcpy(buf->raw + buf->at, src, size);
	buf->at += size;
}

void buf_drop(Weft_Buf **buf_p, size_t size)
{
	Weft_Buf *buf = *buf_p;
	buf->at -= size;
	shrink_if_possible(buf_p, buf);
}

void *buf_pop(void *dest, Weft_Buf **buf_p, size_t size)
{
	Weft_Buf *buf = *buf_p;
	buf->at -= size;
	memcpy(dest, buf->raw + buf->at, size);
	shrink_if_possible(buf_p, buf);

	return dest;
}

void *buf_peek(Weft_Buf *buf, size_t size)
{
	return buf->raw + buf->at - size;
}

void buf_push_byte(Weft_Buf **buf_p, uint8_t byte)
{
	Weft_Buf *buf = *buf_p;
	if (buf->at == buf->cap) {
		buf = realloc_buf(buf, realloc_cap(buf, 1));
		*buf_p = buf;
	}

	buf->raw[buf->at] = byte;
	buf->at += 1;
}

uint8_t buf_pop_byte(Weft_Buf **buf_p)
{
	Weft_Buf *buf = *buf_p;
	buf->at -= 1;
	uint8_t value = buf->raw[buf->at];
	shrink_if_possible(buf_p, buf);

	return value;
}

uint8_t buf_peek_byte(Weft_Buf *buf, size_t index)
{
	uint8_t *byte_p = buf_peek(buf, (index + 1) * sizeof(uint8_t));
	return *byte_p;
}

void buf_push_size(Weft_Buf **buf_p, size_t size)
{
	return buf_push(buf_p, &size, sizeof(size_t));
}

size_t buf_pop_size(Weft_Buf **buf_p)
{
	size_t size;
	buf_pop(&size, buf_p, sizeof(size_t));
	return size;
}

size_t buf_peek_size(Weft_Buf *buf, size_t index)
{
	size_t *size_p = buf_peek(buf, (index + 1) * sizeof(size_t));
	return *size_p;
}

void buf_push_ptr(Weft_Buf **buf_p, void *ptr)
{
	return buf_push(buf_p, &ptr, sizeof(void *));
}

void *buf_pop_ptr(Weft_Buf **buf_p)
{
	void *ptr;
	buf_pop(&ptr, buf_p, sizeof(void *));
	return ptr;
}

void *buf_peek_ptr(Weft_Buf *buf, size_t index)
{
	void **ptr_p = buf_peek(buf, (index + 1) * sizeof(void *));
	return *ptr_p;
}

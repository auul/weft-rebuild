#ifndef WEFT_BUF_H
#define WEFT_BUF_H

#include <stddef.h>
#include <stdint.h>

// Forward Declarations

typedef struct weft_buf Weft_Buf;

// Data Types

struct weft_buf {
	size_t cap;
	size_t at;
	char raw[];
};

// Functions

Weft_Buf *new_buf(size_t cap);
size_t buf_get_cap(const Weft_Buf *buf);
size_t buf_get_at(const Weft_Buf *buf);
void *buf_get_raw(Weft_Buf *buf);
void buf_print(const Weft_Buf *buf, size_t size, void *print_fn);
void buf_clear(Weft_Buf **buf_p);
void buf_push(Weft_Buf **buf_p, const void *src, size_t size);
void buf_drop(Weft_Buf **buf_p, size_t size);
void *buf_pop(void *dest, Weft_Buf **buf_p, size_t size);
void *buf_peek(Weft_Buf *buf, size_t size);

void buf_push_byte(Weft_Buf **buf_p, uint8_t byte);
uint8_t buf_pop_byte(Weft_Buf **buf_p);
uint8_t buf_peek_byte(Weft_Buf *buf, size_t index);
void buf_push_size(Weft_Buf **buf_p, size_t size);
size_t buf_pop_size(Weft_Buf **buf_p);
size_t buf_peek_size(Weft_Buf *buf, size_t index);
void buf_push_ptr(Weft_Buf **buf_p, void *ptr);
void *buf_pop_ptr(Weft_Buf **buf_p);
void *buf_peek_ptr(Weft_Buf *buf, size_t index);

#endif

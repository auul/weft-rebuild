#ifndef WEFT_STR_H
#define WEFT_STR_H

#include <stddef.h>

// Forward Declarations

typedef struct weft_str Weft_Str;

// Data Types

struct weft_str {
	size_t len;
	char ch[];
};

// Functions

Weft_Str *new_str_from_n(const char *src, size_t len);

#endif

#include "str.h"
#include "gc.h"

#include <string.h>

Weft_Str *new_str_from_n(const char *src, size_t len)
{
	Weft_Str *str = gc_alloc(len + 1);
	memcpy(str->ch, src, len);
	str->ch[len] = 0;

	return str;
}

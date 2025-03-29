#include "parse.h"
#include "gc.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define len_of(str) (sizeof(str) - 1)

Weft_ParseFile *new_parse_file(char *path, char *src)
{
	Weft_ParseFile *file = gc_alloc(sizeof(Weft_ParseFile));
	file->path = path;
	file->src = src;

	return file;
}

void parse_file_mark(Weft_ParseFile *file)
{
	if (gc_mark(file)) {
		return;
	}

	gc_mark(file->path);
	gc_mark(file->src);
}

Weft_ParseToken
new_parse_token(Weft_ParseFile *file, const char *src, size_t len)
{
	Weft_ParseToken token = {
		.file = file,
		.src = src,
		.len = len,
	};
	return token;
}

void parse_token_mark(Weft_ParseToken token)
{
	parse_file_mark(token.file);

	switch (token.type) {
	default:
		break;
	}
}

static Weft_ParseToken
tag_error(Weft_ParseFile *file, const char *src, size_t len)
{
	Weft_ParseToken token = new_parse_token(file, src, len);
	token.type = WEFT_PARSE_ERROR;

	return token;
}

static Weft_ParseToken
tag_empty(Weft_ParseFile *file, const char *src, size_t len)
{
	Weft_ParseToken token = new_parse_token(file, src, len);
	token.type = WEFT_PARSE_EMPTY;

	return token;
}

static Weft_ParseToken
tag_char(Weft_ParseFile *file, const char *src, size_t len, uint32_t cnum)
{
	Weft_ParseToken token = new_parse_token(file, src, len);
	token.type = WEFT_PARSE_CHAR;
	token.cnum = cnum;

	return token;
}

static const char *
get_line_start(size_t *line_no_p, const char *src, const char *at)
{
	const char *line = src;
	*line_no_p = 0;

	while (src < at) {
		if (*src == '\n') {
			src++;
			line = src;
			*line_no_p += 1;
		} else {
			src++;
		}
	}
	return line;
}

#define ANSI_FMT_RESET "\e[0m"
#define ANSI_FMT_ERROR "\e[91;1m"  // Red, Bold

static void print_error_msg(
	const char *path, size_t line, size_t col, const char *fmt, va_list args)
{
	fprintf(stderr,
	        "%s:%zu:%zu: " ANSI_FMT_ERROR "error: " ANSI_FMT_RESET,
	        path,
	        line,
	        col);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
}

static void print_error_line_no(size_t line_no)
{
	fprintf(stderr, " %5zu | ", line_no);
}

static void
print_error_context_left(size_t line_no, const char *line, const char *src)
{
	size_t len = src - line;
	print_error_line_no(line_no);
	fprintf(stderr, "%.*s", len, line);
}

static size_t get_line_len(const char *line)
{
	size_t len = 0;
	while (line[len] && line[len] != '\n') {
		len++;
	}
	return len;
}

static void
print_error_context_middle(size_t line_no, const char *src, size_t len)
{
	while (true) {
		size_t line_len = get_line_len(src);
		if (line_len >= len) {
			fprintf(stderr, ANSI_FMT_ERROR "%.*s" ANSI_FMT_RESET, len, src);
			return;
		}

		fprintf(
			stderr, ANSI_FMT_ERROR "%.*s" ANSI_FMT_RESET "\n", line_len, src);
		len -= line_len + 1;
		src += line_len + 1;

		if (len) {
			line_no++;
			print_error_line_no(line_no);
		}
	}
}

static void print_error_context_right(const char *src)
{
	fprintf(stderr, "%.*s\n", get_line_len(src), src);
}

static void print_error_context(size_t line_no,
                                const char *line,
                                const char *src,
                                size_t len)
{
	print_error_context_left(line_no, line, src);
	print_error_context_middle(line_no, src, len);
	print_error_context_right(src + len);
}

Weft_ParseToken parse_error(
	Weft_ParseFile *file, const char *src, size_t len, const char *fmt, ...)
{
	size_t line_no;
	const char *line = get_line_start(&line_no, file->src, src);

	va_list args;
	va_start(args, fmt);
	print_error_msg(file->path, line_no, src - line, fmt, args);
	va_end(args);

	print_error_context(line_no, line, src, len);

	return tag_error(file, src, len);
}

static bool is_line_comment(const char *src)
{
	return src[0] == '#';
}

Weft_ParseToken parse_line_comment(Weft_ParseFile *file, const char *src)
{
	size_t len = len_of("#");
	while (src[len] && src[len] != '\n') {
		len++;
	}
	return tag_empty(file, src, len);
}

Weft_ParseToken parse_empty(Weft_ParseFile *file, const char *src)
{
	size_t len = 0;
	while (true) {
		if (is_line_comment(src + len)) {
			len += parse_line_comment(file, src + len).len;
		} else if (isspace(src[len])) {
			len++;
		} else {
			return tag_empty(file, src, len);
		}
	}
}

static bool is_hex_esc(const char *src)
{
	return src[0] == '\\' && (src[1] == 'x' || src[1] == 'X');
}

static bool is_nibble(char c)
{
	return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || isdigit(c);
}

static unsigned get_nibble(char c)
{
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return c - '0';
}

static long unsigned push_nibble(long unsigned value, char c)
{
	return (value << 4) | get_nibble(c);
}

Weft_ParseToken parse_hex_esc(Weft_ParseFile *file, const char *src)
{
	const char *error_msg = "Expected 0-9|a-f|A-F after '\\x'";
	size_t len = len_of("\\x");
	if (!src[len]) {
		return parse_error(file, src, len, error_msg);
	} else if (!is_nibble(src[len])) {
		return parse_error(file, src, len + 1, error_msg);
	}

	long unsigned value = get_nibble(src[len]);
	len++;

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}
	return tag_char(file, src, len, value);
}

static bool is_lower_utf_esc(const char *src)
{
	return src[0] == '\\' && src[1] == 'u';
}

Weft_ParseToken parse_lower_utf_esc(Weft_ParseFile *file, const char *src)
{
	const char *error_msg = "Expected 0-9|a-f|A-F after '\\u'";
	size_t len = len_of("\\u");
	if (!src[len]) {
		return parse_error(file, src, len, error_msg);
	} else if (!is_nibble(src[len])) {
		return parse_error(file, src, len + 1, error_msg);
	}

	long unsigned value = get_nibble(src[len]);
	len++;

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}
	return tag_char(file, src, len, value);
}

static bool is_upper_utf_esc(const char *src)
{
	return src[0] == '\\' && src[1] == 'U';
}

Weft_ParseToken parse_upper_utf_esc(Weft_ParseFile *file, const char *src)
{
	const char *error_msg = "Expected 0-9|a-f|A-F after '\\U'";
	size_t len = len_of("\\U");
	if (!src[len]) {
		return parse_error(file, src, len, error_msg);
	} else if (!is_nibble(src[len])) {
		return parse_error(file, src, len + 1, error_msg);
	}

	long unsigned value = get_nibble(src[len]);
	len++;

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}

	if (is_nibble(src[len])) {
		value = push_nibble(value, src[len]);
		len++;
	}

	const unsigned UTF32_MAX = 2097151;
	if (value > UTF32_MAX) {
		return parse_error(
			file,
			src,
			len,
			"Unicode value %lu exceeds the maximum UTF-32 value of %u.",
			value,
			UTF32_MAX);
	}
	return tag_char(file, src, len, value);
}

static bool is_dec_esc(const char *src)
{
	return src[0] == '\\' && isdigit(src[1]);
}

static unsigned get_digit(char c)
{
	return c - '0';
}

static long unsigned push_digit(long unsigned value, char c)
{
	return (10 * value) + get_digit(c);
}

Weft_ParseToken parse_dec_esc(Weft_ParseFile *file, const char *src)
{
	size_t len = len_of("\\");
	long unsigned value = get_digit(src[len]);
	len++;

	if (isdigit(src[len])) {
		value = push_digit(value, src[len]);
		len++;
	}

	if (isdigit(src[len])) {
		value = push_digit(value, src[len]);
		len++;

		if (value > UCHAR_MAX) {
			return parse_error(
				file,
				src,
				len,
				"Decimal character literal escape '\\%lu' exceeds max value of "
				"%u. For values in excess of 1 byte, use a multibyte encoding, "
				"or '\\u' and '\\U' to inline values of up to 2 bytes or 4 "
				"bytes respectively",
				value,
				UCHAR_MAX);
		}
	}
	return tag_char(file, src, len, value);
}

static bool is_char_esc(const char *src)
{
	return src[0] == '\\';
}

static unsigned get_esc_char(char c)
{
	switch (c) {
	case 'a':
		return '\a';
	case 'b':
		return '\b';
	case 'e':
		return '\e';
	case 'f':
		return '\f';
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case 'v':
		return '\v';
	default:
		return c;
	}
}

Weft_ParseToken parse_char_esc(Weft_ParseFile *file, const char *src)
{
	size_t len = len_of("\\");
	if (!src[len]) {
		return parse_error(file,
		                   src,
		                   len_of("\\"),
		                   "Expected character escape literal after '\\'");
	}
	return tag_char(file, src, len + 1, get_esc_char(src[len]));
}

static bool is_include_open(const char *src)
{
	return src[0] == '@';
}

Weft_ParseToken parse_include_open(Weft_ParseFile *file, const char *src)
{
	const char *error_msg = "Expected '(' after '@'";
	if (!src[len_of("@")] || isspace(src[len_of("@")])) {
		return parse_error(file, src, len_of("@"), error_msg);
	} else if (src[len_of("@")] != '(') {
		return parse_error(file, src, len_of("@") + 1, error_msg);
	}

	Weft_ParseToken token = new_parse_token(file, src, len_of("@("));
	token.type = WEFT_PARSE_INCLUDE_OPEN;

	return token;
}

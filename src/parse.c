#include "parse.h"
#include "buf.h"
#include "gc.h"
#include "str.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Constants

static const char delim_list[] = "]}):";
static const char restricted_char_list[] = "[]{}():";

// Functions

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

Weft_ParseToken new_parse_token_with_type(Weft_ParseFile *file,
                                          const char *src,
                                          size_t len,
                                          Weft_ParseType type)
{
	Weft_ParseToken token = {
		.file = file,
		.src = src,
		.len = len,
		.type = type,
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

static Weft_ParseToken tag_ptr(Weft_ParseFile *file,
                               const char *src,
                               size_t len,
                               Weft_ParseType type,
                               void *ptr)
{
	Weft_ParseToken token = new_parse_token_with_type(file, src, len, type);
	token.ptr = ptr;

	return token;
}

static Weft_ParseToken
tag_str(Weft_ParseFile *file, const char *src, size_t len, Weft_Str *str)
{
	return tag_ptr(file, src, len, WEFT_PARSE_STR, str);
}

static Weft_ParseToken
tag_num(Weft_ParseFile *file, const char *src, size_t len, double num)
{
	Weft_ParseToken token =
		new_parse_token_with_type(file, src, len, WEFT_PARSE_NUM);
	token.num = num;

	return token;
}

static Weft_ParseToken
tag_word(Weft_ParseFile *file, const char *src, size_t len)
{
	return new_parse_token_with_type(file, src, len, WEFT_PARSE_WORD);
}

static Weft_ParseToken
tag_include(Weft_ParseFile *file, const char *src, size_t len, Weft_Str *path)
{
	return tag_ptr(file, src, len, WEFT_PARSE_INCLUDE, path);
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

static long push_digit(long value, char c)
{
	return (10 * value) + get_digit(c);
}

Weft_ParseToken parse_dec_esc(Weft_ParseFile *file, const char *src)
{
	size_t len = len_of("\\");
	long value = get_digit(src[len]);
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

Weft_ParseToken parse_char_bare(Weft_ParseFile *file, const char *src)
{
	if (!*src) {
		return tag_char(file, src, 0, 0);
	} else if (is_char_esc(src)) {
		if (is_hex_esc(src)) {
			return parse_hex_esc(file, src);
		} else if (is_dec_esc(src)) {
			return parse_dec_esc(file, src);
		} else if (is_lower_utf_esc(src)) {
			return parse_lower_utf_esc(file, src);
		} else if (is_upper_utf_esc(src)) {
			return parse_upper_utf_esc(file, src);
		}
		return parse_char_esc(file, src);
	}
	return tag_char(file, src, 1, src[0]);
}

static bool is_char(const char *src)
{
	return src[0] == '\'';
}

Weft_ParseToken parse_char(Weft_ParseFile *file, const char *src)
{
	const char *error_msg = "Missing terminating ' character";
	size_t len = len_of("'");
	if (!src[len]) {
		return parse_error(file, src, len, error_msg);
	}

	Weft_ParseToken ch = parse_char_bare(file, src + len);
	len += ch.len;

	if (!src[len]) {
		return parse_error(file, src, len, error_msg);
	} else if (src[len] != '\'') {
		return parse_error(
			file, src, len + 1, "Excess characters in character literal");
	}
	len++;

	return tag_char(file, src, len, ch.cnum);
}

static bool is_str(const char *src)
{
	return src[0] == '"';
}

static void push_utf8(Weft_Buf **buf_p, uint32_t c)
{
	const uint8_t UTF8_XBYTE = 128;
	const uint8_t UTF8_2BYTE = 192;
	const uint8_t UTF8_3BYTE = 224;
	const uint8_t UTF8_4BYTE = 240;

	const uint32_t UTF8_XMASK = 192;
	const uint32_t UTF8_1MAX = 127;
	const uint32_t UTF8_2MAX = 2047;
	const uint32_t UTF8_3MAX = 65535;
	const unsigned UTF8_SHIFT = 6;

	uint8_t utf8[4];
	if (c > UTF8_3MAX) {
		utf8[0] = (uint8_t)(c >> (3 * UTF8_SHIFT)) | UTF8_4BYTE;
		utf8[1] = (uint8_t)((c >> (2 * UTF8_SHIFT)) & ~UTF8_XMASK) | UTF8_XBYTE;
		utf8[2] = (uint8_t)((c >> UTF8_SHIFT) & ~UTF8_XMASK) | UTF8_XBYTE;
		utf8[3] = (uint8_t)(c & ~UTF8_XMASK) | UTF8_XBYTE;
		buf_push(buf_p, utf8, 4);
	} else if (c > UTF8_2MAX) {
		utf8[0] = (uint8_t)(c >> (2 * UTF8_SHIFT)) | UTF8_3BYTE;
		utf8[1] = (uint8_t)((c >> UTF8_SHIFT) & ~UTF8_XMASK) | UTF8_XBYTE;
		utf8[2] = (uint8_t)(c & ~UTF8_XMASK) | UTF8_XBYTE;
		buf_push(buf_p, utf8, 3);
	} else if (c > UTF8_1MAX) {
		utf8[0] = (uint8_t)(c >> UTF8_SHIFT) | UTF8_2BYTE;
		utf8[1] = (uint8_t)(c & ~UTF8_XMASK) | UTF8_XBYTE;
		buf_push(buf_p, utf8, 2);
	} else {
		utf8[0] = c;
		buf_push(buf_p, utf8, 1);
	}
}

static void push_char(Weft_Buf **buf_p, uint32_t cnum)
{
	if (cnum > UCHAR_MAX) {
		return push_utf8(buf_p, cnum);
	}

	uint8_t c = cnum;
	buf_push(buf_p, &c, sizeof(uint8_t));
}

Weft_ParseToken parse_str(Weft_ParseFile *file, const char *src)
{
	Weft_Buf *buf = new_buf(1);
	size_t len = len_of("\"");
	while (src[len] != '"') {
		if (!src[len]) {
			return parse_error(
				file, src, len, "Missing terminating \" character");
		}

		Weft_ParseToken ch = parse_char_bare(file, src + len);
		if (ch.type == WEFT_PARSE_CHAR) {
			push_char(&buf, ch.cnum);
		}
		len += ch.len;
	}
	len += len_of("\"");

	Weft_Str *str = new_str_from_n(buf_get_raw(buf), buf_get_at(buf));
	free(buf);

	return tag_str(file, src, len, str);
}

static bool is_num(const char *src)
{
	if (*src == '-') {
		src++;
	}

	if (*src == '.') {
		src++;
	}
	return isdigit(*src);
}

static bool is_delim(const char *src)
{
	return !*src || isspace(*src) || strchr(delim_list, *src);
}

static size_t find_token_end(const char *src, size_t len)
{
	while (!is_delim(src + len)) {
		len++;
	}
	return len;
}

Weft_ParseToken parse_num(Weft_ParseFile *file, const char *src)
{
	long left = 0;
	long right = 0;
	unsigned place = 0;
	bool negative = false;
	bool dot = false;
	size_t len = 0;

	if (src[len] == '-') {
		negative = true;
		len++;
	}

	while (src[len] == '.' || isdigit(src[len])) {
		if (src[len] == '.') {
			if (dot) {
				len = find_token_end(src, len);
				return parse_error(file, src, len, "Invalid number literal");
			}
			dot = true;
		} else if (dot) {
			right = push_digit(right, src[len]);
			place++;
		} else {
			left = push_digit(left, src[len]);
		}
	}

	if (!is_delim(src + len)) {
		len = find_token_end(src, len);
		return parse_error(file, src, len, "Invalid number literal");
	}

	double num = (double)right;
	while (place) {
		num /= 10.0;
		place--;
	}
	num += (double)left;

	if (negative) {
		num = -num;
	}
	return tag_num(file, src, len, num);
}

Weft_ParseToken parse_word(Weft_ParseFile *file, const char *src)
{
	size_t len = 0;
	while (!is_delim(src + len)) {
		if (strchr(restricted_char_list, src[len])) {
			char c = src[len];
			len = find_token_end(src, len);
			return parse_error(
				file, src, len, "Invalid char '%c' in identifier", c);
		}
		len++;
	}
	return tag_word(file, src, len);
}

static bool is_open_paren(const char *src)
{
	return src[0] == '(';
}

Weft_ParseToken parse_open_paren(Weft_ParseFile *file, const char *src)
{
	return new_parse_token_with_type(
		file, src, len_of("("), WEFT_PARSE_OPEN_PAREN);
}

static bool is_close_paren(const char *src)
{
	return src[0] == ')';
}

Weft_ParseToken parse_close_paren(Weft_ParseFile *file, const char *src)
{
	return new_parse_token_with_type(
		file, src, len_of(")"), WEFT_PARSE_OPEN_PAREN);
}

static bool is_open_include(const char *src)
{
	return src[0] == '@';
}

Weft_ParseToken parse_open_include(Weft_ParseFile *file, const char *src)
{
	const char *error_msg = "Expected '(' after '@'";
	if (!src[len_of("@")] || isspace(src[len_of("@")])) {
		return parse_error(file, src, len_of("@"), error_msg);
	} else if (src[len_of("@")] != '(') {
		return parse_error(file, src, len_of("@") + 1, error_msg);
	}
	return new_parse_token_with_type(
		file, src, len_of("@("), WEFT_PARSE_OPEN_INCLUDE);
}

static size_t find_close_paren(const char *src, size_t len)
{
	while (src[len]) {
		if (src[len] == ')') {
			return len + len_of(")");
		}
		len++;
	}
	return len;
}

Weft_ParseToken parse_include(Weft_ParseFile *file, const char *src)
{
	Weft_ParseToken token = parse_open_include(file, src);
	if (token.type == WEFT_PARSE_ERROR) {
		return token;
	}
	size_t len = token.len;

	token = parse_empty(file, src + len);
	len += token.len;

	if (!src[len]) {
		return parse_error(file, src, len, "'@(' without matching ')'");
	} else if (!is_str(src + len)) {
		len = find_close_paren(src, len);
		return parse_error(
			file,
			src,
			len,
			"Expected \"path/to/file\" inside of include statement");
	}

	token = parse_str(file, src + len);
	if (token.type == WEFT_PARSE_ERROR) {
		len = find_close_paren(src, len);
		return parse_error(file, src, len, "Could not include file");
	}
	Weft_Str *path = token.str;
	len += token.len;

	token = parse_empty(file, src + len);
	len += token.len;

	if (!is_close_paren(src + len)) {
		len = find_close_paren(src, len);
		return parse_error(
			file, src, len, "Excess information in include statement");
	}
	len += token.len;

	return tag_include(file, src, len, path);
}

static bool is_open_shuffle(const char *src)
{
	return src[0] == '{';
}

Weft_ParseToken parse_open_shuffle(Weft_ParseFile *file, const char *src)
{
	return new_parse_token_with_type(
		file, src, len_of("{"), WEFT_PARSE_OPEN_SHUFFLE);
}

static bool is_close_shuffle(const char *src)
{
	return src[0] == '}';
}

Weft_ParseToken parse_close_shuffle(Weft_ParseFile *file, const char *src)
{
	return new_parse_token_with_type(
		file, src, len_of("}"), WEFT_PARSE_CLOSE_SHUFFLE);
}

static bool is_shuffle_pivot(const char *src)
{
	return src[0] == '-' && src[1] == '-';
}

static size_t find_shuffle_member_end(const char *src, size_t len)
{
	while (!is_delim(src + len) && !is_shuffle_pivot(src + len)) {
		len++;
	}
	return len;
}

Weft_ParseToken parse_shuffle_member(Weft_ParseFile *file, const char *src)
{
	size_t len = 0;
	while (!is_delim(src + len) && !is_shuffle_pivot(src + len)) {
		if (strchr(restricted_char_list, src[len])) {
			char c = src[len];
			len = find_shuffle_member_end(src, len);
			return parse_error(
				file, src, len, "Invalid char '%c' in identifier", c);
		}
		len++;
	}
	return tag_word(file, src, len);
}

Weft_ParseToken parse_shuffle(Weft_ParseFile *file, const char *src)
{
	Weft_ParseToken token = parse_open_shuffle(file, src);
	size_t len = token.len;

	token = parse_empty(file, src + len);
	len += token.len;

	Weft_Buf *in = new_buf(sizeof(Weft_Str *));
	Weft_Buf *out = new_buf(sizeof(uint8_t));
	bool pivot = false;

	while (!is_close_shuffle(src + len)) {
		if (is_shuffle_pivot(src + len)) {
			pivot = true;
			len += len_of("--");
		} else if (pivot) {

		} else {
			Weft_ParseToken token = parse_shuffle_member(file, src);
			if (token.type != WEFT_PARSE_ERROR) {
				Weft_Str *word = new_str_from_n(token.src, token.len);
				buf_push(&in, &word, sizeof(Weft_Str *));
			}
		}
	}

	token = parse_close_shuffle(file, src + len);
	len += token.len;
}

static bool is_open_list(const char *src)
{
	return src[0] == '[';
}

Weft_ParseToken parse_open_list(Weft_ParseFile *file, const char *src)
{
	return new_parse_token_with_type(
		file, src, len_of("["), WEFT_PARSE_OPEN_LIST);
}

static bool is_close_list(const char *src)
{
	return src[0] == ']';
}

Weft_ParseToken parse_close_list(Weft_ParseFile *file, const char *src)
{
	return new_parse_token_with_type(
		file, src, len_of("]"), WEFT_PARSE_CLOSE_LIST);
}

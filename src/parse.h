#ifndef WEFT_PARSE_H
#define WEFT_PARSE_H

#include <stddef.h>
#include <stdint.h>

// Forward Declarations

typedef struct weft_str Weft_Str;
typedef struct weft_parse_file Weft_ParseFile;
typedef enum weft_parse_type Weft_ParseType;
typedef struct weft_parse_token Weft_ParseToken;

// Data Types

struct weft_parse_file {
	char *path;
	char *src;
};

enum weft_parse_type {
	WEFT_PARSE_ERROR,
	WEFT_PARSE_EMPTY,
	WEFT_PARSE_CHAR,
	WEFT_PARSE_STR,
	WEFT_PARSE_NUM,
	WEFT_PARSE_WORD,
	WEFT_PARSE_OPEN_PAREN,
	WEFT_PARSE_CLOSE_PAREN,
	WEFT_PARSE_OPEN_INCLUDE,
	WEFT_PARSE_INCLUDE,
	WEFT_PARSE_OPEN_SHUFFLE,
	WEFT_PARSE_CLOSE_SHUFFLE,
	WEFT_PARSE_OPEN_LIST,
	WEFT_PARSE_CLOSE_LIST,
};

struct weft_parse_token {
	Weft_ParseFile *file;
	const char *src;
	size_t len;
	Weft_ParseType type;
	union {
		void *ptr;
		uint32_t cnum;
		Weft_Str *str;
		double num;
	};
};

// Functions

Weft_ParseFile *new_parse_file(char *path, char *src);
void parse_file_mark(Weft_ParseFile *file);
Weft_ParseToken
new_parse_token(Weft_ParseFile *file, const char *src, size_t len);
void parse_token_mark(Weft_ParseToken token);
Weft_ParseToken parse_error(
	Weft_ParseFile *file, const char *src, size_t len, const char *fmt, ...);
Weft_ParseToken parse_line_comment(Weft_ParseFile *file, const char *src);
Weft_ParseToken parse_empty(Weft_ParseFile *file, const char *src);

#endif

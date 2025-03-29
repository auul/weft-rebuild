#ifndef WEFT_GC_H
#define WEFT_GC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward Declarations

typedef struct weft_gc Weft_GC;

// Data Structures

struct weft_gc {
	uintptr_t prev;
	char ptr[];
};

// Constants

static const size_t WEFT_GC_INIT_TRIGGER = 8;

// Functions

void *gc_alloc(size_t size);
bool gc_mark(void *ptr);
size_t gc_get_count(void);
bool gc_is_ready(void);
void gc_collect(void);

#endif

#include "gc.h"

#include <stdio.h>
#include <stdlib.h>

// Globals

static Weft_GC *g_heap;
size_t g_trigger = WEFT_GC_INIT_TRIGGER;
size_t g_count = 0;

// Functions

int gc_error(void)
{
	perror("Could not allocate memory");
	return 1;
}

static Weft_GC *get_tag(void *ptr)
{
	return (Weft_GC *)ptr - 1;
}

static Weft_GC *get_tag_prev(Weft_GC *tag)
{
	return (Weft_GC *)(tag->prev >> 1);
}

static void set_tag_prev(Weft_GC *tag, Weft_GC *prev)
{
	tag->prev = (uintptr_t)prev << 1;
}

static bool is_tag_marked(const Weft_GC *tag)
{
	return tag->prev & 1;
}

static void mark_tag(Weft_GC *tag)
{
	tag->prev |= 1;
}

static void unmark_tag(Weft_GC *tag)
{
	tag->prev &= ~(uintptr_t)1;
}

void *gc_alloc(size_t size)
{
	Weft_GC *tag = malloc(sizeof(Weft_GC) + size);
	if (!tag) {
		exit(gc_error());
	}

	set_tag_prev(tag, g_heap);
	g_heap = tag;
	g_count++;

	return tag->ptr;
}

bool gc_mark(void *ptr)
{
	if (!ptr) {
		return true;
	}

	Weft_GC *tag = get_tag(ptr);
	if (is_tag_marked(tag)) {
		return true;
	}
	mark_tag(tag);

	return false;
}

size_t gc_get_count(void)
{
	return g_count;
}

bool gc_is_ready(void)
{
	return g_count >= g_trigger;
}

static Weft_GC *pop_tag(Weft_GC *tag)
{
	Weft_GC *prev = get_tag_prev(tag);
	free(tag);
	g_count--;

	return prev;
}

static void set_trigger(size_t count)
{
	g_trigger = 2 * count;
}

void gc_collect(void)
{
	while (g_heap && !is_tag_marked(g_heap)) {
		g_heap = pop_tag(g_heap);
	}

	for (Weft_GC *tag = g_heap; tag; tag = get_tag_prev(tag)) {
		while (get_tag_prev(tag) && !is_tag_marked(get_tag_prev(tag))) {
			set_tag_prev(tag, pop_tag(get_tag_prev(tag)));
		}
		unmark_tag(tag);
	}
	set_trigger(g_count);
}

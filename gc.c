#include "gc.h"
#include <stdlib.h>

static gcptr_t *head = NULL;
static gcptr_t *tail = NULL;

gcptr_t *gc_malloc_ptr(size_t size) {
  gcptr_t *item = malloc(sizeof *item);
  item->ptr = malloc(size);
  item->next = NULL;

  if (!head) {
    head = item;
    tail = head;
  } else {
    tail->next = item;
    tail = tail->next;
  }

  return item;
}

void *gc_malloc(size_t size) {
  gcptr_t *item = gc_malloc_ptr(size);
  return item->ptr;
}

void gc_free(void) {
  gcptr_t *tmp = head;

  while (tmp != NULL) {
    free(tmp->ptr);
    head = head->next;
    free(tmp);
    tmp = head;
  }
}

#ifndef GC_H_
#define GC_H_

#include <stdlib.h>

typedef struct gcptr {
  void *ptr;
  struct gcptr *next;
} gcptr_t;

gcptr_t *gc_malloc_ptr(size_t size);
void *gc_malloc(size_t size);
void gc_free(void);

#endif // GC_H_

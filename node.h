#ifndef T2J_H
#define T2J_H
#include <stdio.h>

enum nodeType { NODE_STR, NODE_INT, NODE_LIST, NODE_DICT };

typedef struct node {
  enum nodeType type;
  void *value;
} node_t;

typedef struct node_str {
  size_t size;
  char *value;
} node_str_t;

typedef struct node_int {
  int value;
} node_int_t;

typedef struct node_list {
  size_t size;
  node_t **nodes;
} node_list_t;

typedef struct node_dict_entry {
  node_t *key;
  node_t *value;
} node_dict_entry_t;

typedef struct node_dict {
  size_t size;
  node_dict_entry_t **entries;
} node_dict_t;

typedef struct reader {
  FILE *fp;
} reader_t;

node_t *consume(void);
node_t *consume_str(void);
node_t *consume_int(void);
node_t *consume_list(void);
node_t *new_list(void);
node_t *consume_dict(void);
node_t *new_dict(void);
node_dict_entry_t *consume_dict_entry(void);

#endif // T2J_H

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include "gc.h"
#include "node.h"
#include "print.h"
#include <string.h>

// TODO: in general some more safey checks are needed
// TODO: add -o for output to file

reader_t reader;

// NOTE: could add a 'atext'?
void die(const char *reason) {
  if (reader.fp)
    fclose(reader.fp);
  gc_free();
  perror(reason);
  exit(1);
}

char peek(int n) {
  if (n < 1)
    die("peek");

  fseek(reader.fp, n - 1, SEEK_CUR);
  char c = fgetc(reader.fp);
  fseek(reader.fp, -n, SEEK_CUR);
  return c;
}

void skip(void) { fseek(reader.fp, 1, SEEK_CUR); }
char read(void) { return fgetc(reader.fp); }

node_t *consume_str(void) {
  char c = peek(1);

  if (c == ':')
    die("t2j: invalid string (format)");

  char size_as_str[16];
  int i = 0;

  while ((c = read()) != EOF && c != ':')
    size_as_str[i++] = c;

  if (c == EOF)
    die("t2j: invalid string (EOF)");

  size_as_str[i] = '\0';
  // TODO: might wanna check for errors here
  int size = atoi(size_as_str);

  if (size < 1)
    die("t2j: invalid string (size < 1)");

  char *str = gc_malloc(size + 1);

  // NOTE: we don't check for EOF since some binary data might be set to -1
  i = 0;
  while (i < size)
    str[i++] = read();

  str[i] = '\0';

  node_str_t *v = gc_malloc(sizeof *v);
  v->size = size;
  v->value = str;

  node_t *n = gc_malloc(sizeof *n);
  n->type = NODE_STR;
  n->value = v;

  return n;
}

node_t *consume_int(void) {
  // Skip the leading 'i'
  skip();

  char c;
  c = peek(1);

  if (c == EOF || c == 'e')
    die("t2j: invalid integer (EOF)");

  if (c == '-' && peek(2) == 'e')
    die("t2j: invalid integer (negative)");

  if (c == '0' && peek(2) != 'e')
    die("t2j: invalid integer (leading zero)");

  char int_as_str[16];
  int i = 0;

  while ((c = read()) != EOF && c != 'e')
    int_as_str[i++] = c;

  if (c == EOF)
    die("t2j: invalid integer (EOF)");

  int_as_str[i] = '\0';

  node_int_t *v = gc_malloc(sizeof *v);
  // TODO: might wanna check for errors here
  v->value = atoi(int_as_str);

  node_t *n = gc_malloc(sizeof *n);
  n->type = NODE_INT;
  n->value = v;

  return n;
}

node_t *new_list(void) {
  node_list_t *v = gc_malloc(sizeof *v);
  v->size = 0;
  v->nodes = NULL;

  node_t *n = gc_malloc(sizeof *n);
  n->type = NODE_LIST;
  n->value = v;

  return n;
}

node_t *consume_list(void) {
  // Skip the leading 'l'
  skip();

  char c = peek(1);
  if (c == EOF)
    die("t2j: invalid list (EOF)");

  if (c == 'e')
    return new_list();

  node_list_t *v = gc_malloc(sizeof *v);
  v->size = 0;

  size_t lsize = 8;
  gcptr_t *nodes = gc_malloc_ptr(sizeof(node_t) * (lsize + 1));
  v->nodes = nodes->ptr;

  node_t *n = gc_malloc(sizeof *n);
  n->type = NODE_LIST;
  n->value = v;

  node_t *item;
  while (c != EOF && c != 'e' && (item = consume()) != NULL) {
    v->nodes[v->size++] = item;

    if (v->size == lsize) {
      lsize *= 2;
      v->nodes = realloc(v->nodes, sizeof(node_t) * (lsize + 1));
    }

    c = peek(1);
  }

  if (v->size != lsize)
    v->nodes = realloc(v->nodes, sizeof(node_t) * (v->size + 1));

  nodes->ptr = v->nodes;

  if (c == EOF || c != 'e')
    die("t2j: invalid list (EOF)");

  // Skip the trailing 'e'
  skip();

  return n;
}

node_dict_entry_t *consume_dict_entry(void) {
  node_dict_entry_t *e = gc_malloc(sizeof *e);
  e->key = consume_str();
  e->value = consume();
  return e;
}

node_t *new_dict(void) {
  node_dict_t *v = gc_malloc(sizeof *v);
  v->size = 0;
  v->entries = NULL;

  node_t *n = gc_malloc(sizeof *n);
  n->type = NODE_DICT;
  n->value = v;

  return n;
}

node_t *consume_dict(void) {
  // Skip the leading 'd'
  skip();

  char c = peek(1);
  if (c == EOF)
    die("t2j: invalid dict (EOF)");

  if (c == 'e')
    return new_dict();

  node_dict_t *v = gc_malloc(sizeof *v);
  v->size = 0;

  size_t dsize = 8;
  gcptr_t *entries = gc_malloc_ptr(sizeof(node_dict_entry_t) * (dsize + 1));
  v->entries = entries->ptr;

  node_t *n = gc_malloc(sizeof *n);
  n->type = NODE_DICT;
  n->value = v;

  node_dict_entry_t *entry;
  while (c != EOF && c != 'e' && (entry = consume_dict_entry()) != NULL) {
    v->entries[v->size++] = entry;

    if (v->size == dsize) {
      dsize *= 2;
      v->entries = realloc(v->entries, sizeof(node_dict_entry_t) * (dsize + 1));
    }

    c = peek(1);
  }

  if (v->size != dsize)
    v->entries = realloc(v->entries, sizeof(node_dict_entry_t) * (v->size + 1));

  entries->ptr = v->entries;

  if (c == EOF || c != 'e')
    die("t2j: invalid dict (EOF)");

  // Skip the trailing 'e'
  skip();

  return n;
}

node_t *consume(void) {
  char c = peek(1);

  if (c == EOF)
    return NULL;

  switch (c) {
  case 'd':
    return consume_dict();
  case 'l':
    return consume_list();
  case 'i':
    return consume_int();
  // NOTE: we could verify that 'c' is a digit here
  default:
    return consume_str();
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2)
    die("t2j: not enough arguments");

  int pretty_print = 0;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      char *filename = argv[i];
      reader.fp = fopen(filename, "r");

      if (!reader.fp)
        die("t2j: unable to open");

    } else if (strcmp(argv[i], "--pretty-print") == 0) {
      pretty_print = 1;
    }
  }

  node_t *root = consume();
  fclose(reader.fp);

  if (root == NULL)
    die("t2j: unable to parse");

  print_json(root, 0, pretty_print);
  gc_free();

  return 0;
}

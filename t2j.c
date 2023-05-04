#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: add some naive garbage collector to keep track of memory more easily
// TODO: don't reallocate on each new list/dict value, start with 8 and double
//       it when needed then resize to fit
// TODO: in general some more safey checks are needed
// TODO: make it so we free (perhaps with the GC) memory when we `die` as well

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

int PRETTY_PRINT = 0;

// Prototypes
node_t *consume(void);
void free_node(node_t *n);
void print_json(node_t *n, int indent);

reader_t reader;

// NOTE: could add a 'atext'?
void die(const char *reason) {
  if (reader.fp)
    fclose(reader.fp);
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

  char *str = malloc(size + 1);

  i = 0;
  while (i < size)
    str[i++] = read();

  /* NOTE We can't check for EOF since some binary data might be set to -1 */
  /* while (i < size && (c = read()) != EOF) */
  /*   str[i++] = c; */
  /* if (c == EOF) */
  /*   die("t2j: invalid string (EOF)"); */

  str[i] = '\0';

  node_str_t *v = malloc(sizeof *v);
  v->size = size;
  v->value = str;

  node_t *n = malloc(sizeof *n);
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

  node_int_t *v = malloc(sizeof *v);
  // TODO: might wanna check for errors here
  v->value = atoi(int_as_str);

  node_t *n = malloc(sizeof *n);
  n->type = NODE_INT;
  n->value = v;

  return n;
}

node_t *empty_list(void) {
  node_list_t *v = malloc(sizeof *v);
  v->size = 0;
  v->nodes = NULL;

  node_t *n = malloc(sizeof *n);
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
    return empty_list();

  node_list_t *v = malloc(sizeof *v);
  v->size = 0;
  v->nodes = malloc(sizeof(node_t) * (v->size + 1));

  node_t *n = malloc(sizeof *n);
  n->type = NODE_LIST;
  n->value = v;

  node_t *item;
  while (c != EOF && c != 'e' && (item = consume()) != NULL) {
    v->nodes[v->size++] = item;
    v->nodes = realloc(v->nodes, sizeof(node_t) * (v->size + 1));
    c = peek(1);
  }

  if (c == EOF || c != 'e')
    die("t2j: invalid list (EOF)");

  // Skip the trailing 'e'
  skip();

  return n;
}

node_dict_entry_t *consume_dict_entry(void) {
  node_dict_entry_t *e = malloc(sizeof(*e));
  e->key = consume_str();
  e->value = consume();
  return e;
}

node_t *empty_dict(void) {
  node_dict_t *v = malloc(sizeof *v);
  v->size = 0;
  v->entries = NULL;

  node_t *n = malloc(sizeof *n);
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
    return empty_dict();

  node_dict_t *v = malloc(sizeof *v);
  v->size = 0;
  v->entries = malloc(sizeof(node_dict_entry_t) * (v->size + 1));

  node_t *n = malloc(sizeof *n);
  n->type = NODE_DICT;
  n->value = v;

  node_dict_entry_t *entry;
  while (c != EOF && c != 'e' && (entry = consume_dict_entry()) != NULL) {
    v->entries[v->size++] = entry;
    v->entries = realloc(v->entries, sizeof(node_dict_entry_t) * (v->size + 1));
    c = peek(1);
  }

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

void free_node(node_t *n) {
  switch (n->type) {
  case NODE_INT:
    // Nothing.
    break;
  case NODE_STR: {
    node_str_t *str = n->value;
    free(str->value);
  } break;
  case NODE_LIST: {
    node_list_t *list = n->value;
    while (list->size--)
      free_node(list->nodes[list->size]);
    free(list->nodes);
  } break;
  case NODE_DICT: {
    node_dict_t *dict = n->value;
    while (dict->size--) {
      node_dict_entry_t *entry = dict->entries[dict->size];
      free_node(entry->key);
      free_node(entry->value);
      free(entry);
    }
    free(dict->entries);
  } break;
  default:
    die("t2j: unknown node (free)");
  }

  free(n->value);
  free(n);
}

void indent_line(int n) {
  while (n--)
    printf(" ");
}

void print_json(node_t *n, int indent) {
  switch (n->type) {
  case NODE_STR: {
    node_str_t *s = n->value;
    // TODO: do we need to escape strings upon output?
    printf("\"%s\"", s->value);
    break;
  }
  case NODE_INT: {
    node_int_t *i = n->value;
    printf("%d", i->value);
    break;
  }
  case NODE_LIST: {
    printf(PRETTY_PRINT ? "[\n" : "[");

    node_list_t *list = n->value;
    for (size_t i = 0; i < list->size; i++) {
      if (PRETTY_PRINT)
        indent_line(indent + 2);

      print_json(list->nodes[i], PRETTY_PRINT ? indent + 2 : 0);

      if ((i + 1) < list->size)
        printf(", ");

      if (PRETTY_PRINT)
        printf("\n");
    }

    if (PRETTY_PRINT)
      indent_line(indent);

    printf("]");
    break;
  }
  case NODE_DICT: {
    printf(PRETTY_PRINT ? "{\n" : "{");

    node_dict_t *dict = n->value;
    for (size_t i = 0; i < dict->size; i++) {
      node_dict_entry_t *entry = dict->entries[i];

      if (PRETTY_PRINT)
        indent_line(indent + 2);

      print_json(entry->key, 0);
      printf(": ");

      print_json(entry->value, PRETTY_PRINT ? indent + 2 : 0);

      if ((i + 1) < dict->size)
        printf(", ");

      if (PRETTY_PRINT)
        printf("\n");
    }

    if (PRETTY_PRINT)
      indent_line(indent);

    printf("}");
    break;
  }
  default:
    printf("unknown\n");
    break;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2)
    die("t2j: not enough arguments");

  // TODO: add -o for output to file
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      char *filename = argv[i];
      reader.fp = fopen(filename, "r");

      if (!reader.fp)
        die("t2j: unable to open");

    } else if (strcmp(argv[i], "--pretty-print") == 0) {
      PRETTY_PRINT = 1;
    }
  }

  node_t *root = consume();
  fclose(reader.fp);

  if (root == NULL)
    die("t2j: unable to parse");

  print_json(root, 0);
  free_node(root);

  return 0;
}

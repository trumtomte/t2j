#include "node.h"
#include <stdio.h>

static void indent_line(int n) {
  while (n--)
    printf(" ");
}

void print_json(node_t *n, int indent, int pretty) {
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
    printf(pretty ? "[\n" : "[");

    node_list_t *list = n->value;
    for (size_t i = 0; i < list->size; i++) {
      if (pretty)
        indent_line(indent + 2);

      print_json(list->nodes[i], pretty ? indent + 2 : 0, pretty);

      if ((i + 1) < list->size)
        printf(", ");

      if (pretty)
        printf("\n");
    }

    if (pretty)
      indent_line(indent);

    printf("]");
    break;
  }
  case NODE_DICT: {
    printf(pretty ? "{\n" : "{");

    node_dict_t *dict = n->value;
    for (size_t i = 0; i < dict->size; i++) {
      node_dict_entry_t *entry = dict->entries[i];

      if (pretty)
        indent_line(indent + 2);

      print_json(entry->key, 0, pretty);
      printf(": ");

      print_json(entry->value, pretty ? indent + 2 : 0, pretty);

      if ((i + 1) < dict->size)
        printf(", ");

      if (pretty)
        printf("\n");
    }

    if (pretty)
      indent_line(indent);

    printf("}");
    break;
  }
  default:
    printf("unknown\n");
    break;
  }
}

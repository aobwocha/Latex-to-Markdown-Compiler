#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "latex_compiler.h"

#define MAX_TOTAL_SUBSTITUTIONS 256

typedef struct {
    char *name;
    NodeList body;
    int param_count;
} MacroDef;

typedef struct {
    MacroDef *defs;
    size_t count;
    size_t capacity;
} MacroTable;


static const char *NO_EXPAND_LIST[] = { "\\ressection" };
static const size_t NO_EXPAND_COUNT = sizeof(NO_EXPAND_LIST) / sizeof(NO_EXPAND_LIST[0]);

static bool is_no_expand(const char *name) {
    for (size_t i = 0; i < NO_EXPAND_COUNT; i++) {
        if (strcmp(name, NO_EXPAND_LIST[i]) == 0) return true;
    }
    return false;
}

static Node *deep_copy_node(const Node *src);
static void nl_append(NodeList *list, Node *node) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        list->data = realloc(list->data, list->capacity * sizeof(Node *));
    }
    list->data[list->count++] = node;
}

static Node *make_text_node(const char *start, size_t len) {
    Node *n = malloc(sizeof(Node));
    n->type = NODE_TEXT;
    n->as.text.value = malloc(len + 1);
    memcpy(n->as.text.value, start, len);
    n->as.text.value[len] = '\0';
    return n;
}

static void substitute_text_value(const char *value, const NodeList *const *arg_children,
                                   int param_count, NodeList *out) {
    if (!value) return;
    size_t len = strlen(value);
    size_t seg_start = 0;
    for (size_t i = 0; i < len; i++) {
        if (value[i] == '#' && i + 1 < len && value[i + 1] >= '1' && value[i + 1] <= '9') {
            int arg_index = (value[i + 1] - '1');
            if (arg_index < param_count) {
                if (i > seg_start) nl_append(out, make_text_node(value + seg_start, i - seg_start));
                const NodeList *arg_list = arg_children[arg_index];
                for (size_t k = 0; k < arg_list->count; k++) {
                    nl_append(out, deep_copy_node(arg_list->data[k]));
                }
                i += 1;
                seg_start = i + 1;
            }
        }
    }
    if (seg_start < len) nl_append(out, make_text_node(value + seg_start, len - seg_start));
}

static NodeList substitute_body_list(const NodeList *template_list,
                                      const NodeList *const *arg_children, int param_count) {
    NodeList out = {NULL, 0, 0};
    for (size_t i = 0; i < template_list->count; i++) {
        Node *node = template_list->data[i];
        switch (node->type) {
        case NODE_TEXT:
            substitute_text_value(node->as.text.value, arg_children, param_count, &out);
            break;
        case NODE_COMMAND: {
            Node *copy = malloc(sizeof(Node));
            copy->type = NODE_COMMAND;
            copy->as.command.name = strdup(node->as.command.name);
            copy->as.command.arg_count = node->as.command.arg_count;
            copy->as.command.arg_capacity = node->as.command.arg_count;
            copy->as.command.args = node->as.command.arg_count
                ? malloc(node->as.command.arg_count * sizeof(CommandArg)) : NULL;
            for (size_t a = 0; a < node->as.command.arg_count; a++) {
                copy->as.command.args[a].type = node->as.command.args[a].type;
                copy->as.command.args[a].children =
                    substitute_body_list(&node->as.command.args[a].children, arg_children, param_count);
            }
            nl_append(&out, copy);
            break;
        }
        case NODE_GROUP: {
            Node *copy = malloc(sizeof(Node));
            copy->type = NODE_GROUP;
            copy->as.group.children = substitute_body_list(&node->as.group.children, arg_children, param_count);
            nl_append(&out, copy);
            break;
        }
        case NODE_ENVIRONMENT: {
            Node *copy = malloc(sizeof(Node));
            copy->type = NODE_ENVIRONMENT;
            copy->as.environment.name = strdup(node->as.environment.name);
            copy->as.environment.opt_args =
                substitute_body_list(&node->as.environment.opt_args, arg_children, param_count);
            copy->as.environment.children =
                substitute_body_list(&node->as.environment.children, arg_children, param_count);
            nl_append(&out, copy);
            break;
        }
        }
    }
    return out;
}

static NodeList deep_copy_node_list(const NodeList *src) {
    NodeList out = {NULL, 0, 0};
    if (!src || src->count == 0) return out;
    out.data = malloc(src->count * sizeof(Node *));
    out.count = src->count;
    out.capacity = src->count;
    for (size_t i = 0; i < src->count; i++) {
        out.data[i] = deep_copy_node(src->data[i]);
    }
    return out;
}

static Node *deep_copy_node(const Node *src) {
    if (!src) return NULL;
    Node *n = malloc(sizeof(Node));
    n->type = src->type;
    switch (src->type) {
    case NODE_TEXT:
        n->as.text.value = strdup(src->as.text.value ? src->as.text.value : "");
        break;
    case NODE_COMMAND:
        n->as.command.name = strdup(src->as.command.name);
        n->as.command.arg_count = src->as.command.arg_count;
        n->as.command.arg_capacity = src->as.command.arg_count;
        n->as.command.args = src->as.command.arg_count
            ? malloc(src->as.command.arg_count * sizeof(CommandArg)) : NULL;
        for (size_t i = 0; i < src->as.command.arg_count; i++) {
            n->as.command.args[i].type = src->as.command.args[i].type;
            n->as.command.args[i].children = deep_copy_node_list(&src->as.command.args[i].children);
        }
        break;
    case NODE_GROUP:
        n->as.group.children = deep_copy_node_list(&src->as.group.children);
        break;
    case NODE_ENVIRONMENT:
        n->as.environment.name = strdup(src->as.environment.name);
        n->as.environment.opt_args = deep_copy_node_list(&src->as.environment.opt_args);
        n->as.environment.children = deep_copy_node_list(&src->as.environment.children);
        break;
    }
    return n;
}

static void add_macro(MacroTable *table, const char *name, NodeList body, int param_count) {
    if (table->count >= table->capacity) {
        table->capacity = table->capacity == 0 ? 8 : table->capacity * 2;
        table->defs = realloc(table->defs, table->capacity * sizeof(MacroDef));
    }
    table->defs[table->count].name = strdup(name);
    table->defs[table->count].body = body;
    table->defs[table->count].param_count = param_count;
    table->count++;
}

static const MacroDef *find_macro(const MacroTable *table, const char *name) {
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->defs[i].name, name) == 0) return &table->defs[i];
    }
    return NULL;
}

static void free_macro_table(MacroTable *table) {
    for (size_t i = 0; i < table->count; i++) {
        free(table->defs[i].name);
        for (size_t j = 0; j < table->defs[i].body.count; j++) {
            free_node(table->defs[i].body.data[j]);
        }
        free(table->defs[i].body.data);
    }
    free(table->defs);
}

static void collect_macro_definitions(const NodeList *root, MacroTable *table) {
    for (size_t i = 0; i < root->count; i++) {
        Node *n = root->data[i];
        if (n->type != NODE_COMMAND) continue;
        if (strcmp(n->as.command.name, "\\newcommand") != 0) continue;
        if (n->as.command.arg_count != 2 && n->as.command.arg_count != 3) continue;

        NodeList *name_children = &n->as.command.args[0].children;
        if (name_children->count != 1 || name_children->data[0]->type != NODE_COMMAND) {
            continue;
        }
        const char *macro_name = name_children->data[0]->as.command.name;
        if (is_no_expand(macro_name)) continue;

        if (n->as.command.arg_count == 2) {
            NodeList body = deep_copy_node_list(&n->as.command.args[1].children);
            add_macro(table, macro_name, body, 0);
            continue;
        }

        if (n->as.command.args[1].type != ARG_OPTIONAL) continue;
        NodeList *count_children = &n->as.command.args[1].children;
        if (count_children->count != 1 || count_children->data[0]->type != NODE_TEXT) {
            continue;
        }
        int param_count = atoi(count_children->data[0]->as.text.value);
        if (param_count < 1 || param_count > 9) continue;

        NodeList body = deep_copy_node_list(&n->as.command.args[2].children);
        add_macro(table, macro_name, body, param_count);
    }
}

static void splice_replace(NodeList *list, size_t index, NodeList replacement) {
    size_t old_count = list->count;
    size_t new_total = old_count - 1 + replacement.count;
    Node **new_data = new_total ? malloc(new_total * sizeof(Node *)) : NULL;

    for (size_t i = 0; i < index; i++) new_data[i] = list->data[i];
    for (size_t i = 0; i < replacement.count; i++) new_data[index + i] = replacement.data[i];
    for (size_t i = index + 1; i < old_count; i++) {
        new_data[index + replacement.count + (i - index - 1)] = list->data[i];
    }

    free(list->data);
    free(replacement.data);
    list->data = new_data;
    list->count = new_total;
    list->capacity = new_total;
}

static void expand_node_list(NodeList *list, const MacroTable *table, int *subst_budget) {
    for (size_t i = 0; i < list->count; ) {
        Node *node = list->data[i];

        if (node->type == NODE_COMMAND) {
            const MacroDef *def = find_macro(table, node->as.command.name);
            if (def && (size_t)def->param_count == node->as.command.arg_count) {
                if (*subst_budget <= 0) {
                    fprintf(stderr,
                        "Warning: macro expansion budget exceeded at %s "
                        "(possible cyclic \\newcommand reference) -- left unexpanded\n",
                        node->as.command.name);
                    i++;
                    continue;
                }
                (*subst_budget)--;

                NodeList replacement;
                if (def->param_count == 0) {
                    replacement = deep_copy_node_list(&def->body);
                } else {
                    const NodeList *arg_ptrs[9];
                    for (int a = 0; a < def->param_count; a++) {
                        arg_ptrs[a] = &node->as.command.args[a].children;
                    }
                    replacement = substitute_body_list(&def->body, arg_ptrs, def->param_count);
                }
                free_node(node);
                splice_replace(list, i, replacement);
                continue;
            }
        }

        switch (node->type) {
        case NODE_GROUP:
            expand_node_list(&node->as.group.children, table, subst_budget);
            break;
        case NODE_ENVIRONMENT:
            expand_node_list(&node->as.environment.opt_args, table, subst_budget);
            expand_node_list(&node->as.environment.children, table, subst_budget);
            break;
        case NODE_COMMAND:
            for (size_t a = 0; a < node->as.command.arg_count; a++) {
                expand_node_list(&node->as.command.args[a].children, table, subst_budget);
            }
            break;
        case NODE_TEXT:
        default:
            break;
        }
        i++;
    }
}

void expand_macros(NodeList *root) {
    MacroTable table = {NULL, 0, 0};
    collect_macro_definitions(root, &table);

    Node *doc_env = NULL;
    for (size_t i = 0; i < root->count; i++) {
        if (root->data[i]->type == NODE_ENVIRONMENT &&
            strcmp(root->data[i]->as.environment.name, "document") == 0) {
            doc_env = root->data[i];
            break;
        }
    }

    int subst_budget = MAX_TOTAL_SUBSTITUTIONS;
    if (doc_env) {
        expand_node_list(&doc_env->as.environment.children, &table, &subst_budget);
    } else {
        expand_node_list(root, &table, &subst_budget);
    }

    free_macro_table(&table);
}
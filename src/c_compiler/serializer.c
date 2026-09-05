#define _POSIX_C_SOURCE 200809L  /* required for open_memstream under -std=c99 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "latex_compiler.h"

static bool is_visual_layout_command(const char *name) {
    if (!name) return false;
    const char *ignored[] = {
        "\\vspace", "\\hspace", "\\vfill", "\\small", "\\LARGE",
        "\\Large", "\\large", "\\normalsize", "\\bfseries", "\\itshape",
        "\\centering", "\\clearpage", "\\newpage", "\\label", "\\ ",
        "\\fontsize", "\\selectfont", "\\linespread"
    };
    size_t count = sizeof(ignored) / sizeof(ignored[0]);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(name, ignored[i]) == 0) return true;
    }
    return false;
}

static char *unescape_latex_chars(const char *value) {
    if (!value) return strdup("");
    size_t len = strlen(value);
    char *out = malloc(len + 1);
    memcpy(out, value, len + 1);
    return out;
}

static void serialize_node_markdown(const Node *node, FILE *out);
static void serialize_node_list_markdown(const NodeList *list, FILE *out);

static char *render_nodes_to_markdown(const NodeList *list) {
    char *buf = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buf, &size);
    if (!mem) return strdup("");
    serialize_node_list_markdown(list, mem);
    fclose(mem);
    return buf ? buf : strdup("");
}

static void serialize_children_as_list(const NodeList *list, FILE *out) {
    bool in_item = false;
    for (size_t i = 0; i < list->count; i++) {
        Node *n = list->data[i];
        if (n->type == NODE_COMMAND && strcmp(n->as.command.name, "\\item") == 0) {
            if (in_item) fprintf(out, "\n");
            fprintf(out, "- ");
            in_item = true;
            continue;
        }
        serialize_node_markdown(n, out);
    }
    if (in_item) fprintf(out, "\n");
}

static void serialize_node_list_markdown(const NodeList *list, FILE *out) {
    for (size_t i = 0; i < list->count; i++) {
        serialize_node_markdown(list->data[i], out);
    }
}

static void serialize_node_markdown(const Node *node, FILE *out) {
    if (!node) return;

    switch (node->type) {

    case NODE_TEXT: {
        char *unescaped = unescape_latex_chars(node->as.text.value);
        fputs(unescaped, out);
        free(unescaped);
        break;
    }

    case NODE_COMMAND: {
        const char *name = node->as.command.name;

        if (is_visual_layout_command(name)) {
            return;
        }

        if (strcmp(name, "\\hfill") == 0) {
            fprintf(out, " \xe2\x80\x94 ");
            return;
        }

        if (strcmp(name, "\\\\") == 0) {
            fprintf(out, "  \n");
            return;
        }

        if (strcmp(name, "\\section") == 0 || strcmp(name, "\\ressection") == 0) {
            char *title = node->as.command.arg_count > 0
                ? render_nodes_to_markdown(&node->as.command.args[0].children)
                : strdup("");
            fprintf(out, "\n## %s\n\n", title);
            free(title);
            return;
        }

        if (strcmp(name, "\\subsection") == 0) {
            char *title = node->as.command.arg_count > 0
                ? render_nodes_to_markdown(&node->as.command.args[0].children)
                : strdup("");
            fprintf(out, "\n### %s\n\n", title);
            free(title);
            return;
        }

        if (strcmp(name, "\\textbf") == 0) {
            char *val = node->as.command.arg_count > 0
                ? render_nodes_to_markdown(&node->as.command.args[0].children)
                : strdup("");
            fprintf(out, "**%s**", val);
            free(val);
            return;
        }

        if (strcmp(name, "\\textit") == 0) {
            char *val = node->as.command.arg_count > 0
                ? render_nodes_to_markdown(&node->as.command.args[0].children)
                : strdup("");
            fprintf(out, "*%s*", val);
            free(val);
            return;
        }

        if (strcmp(name, "\\href") == 0) {
            char *url = node->as.command.arg_count > 0
                ? render_nodes_to_markdown(&node->as.command.args[0].children) : strdup("");
            char *text = node->as.command.arg_count > 1
                ? render_nodes_to_markdown(&node->as.command.args[1].children) : strdup("");
            fprintf(out, "[%s](%s)", text, url);
            free(url);
            free(text);
            return;
        }

        if (node->as.command.arg_count > 0) {
            fprintf(out, "\n**[%s]** ", name);
            for (size_t i = 0; i < node->as.command.arg_count; i++) {
                char *arg_text = render_nodes_to_markdown(&node->as.command.args[i].children);
                fprintf(out, "%s", arg_text);
                if (i + 1 < node->as.command.arg_count) fprintf(out, " \xe2\x80\x94 ");
                free(arg_text);
            }
            fprintf(out, "\n");
        }
        return;
    }

    case NODE_GROUP: {
        serialize_node_list_markdown(&node->as.group.children, out);
        break;
    }

    case NODE_ENVIRONMENT: {
        const char *env_name = node->as.environment.name;
        if (strcmp(env_name, "itemize") == 0 || strcmp(env_name, "enumerate") == 0) {
            fprintf(out, "\n");
            serialize_children_as_list(&node->as.environment.children, out);
            fprintf(out, "\n");
        } else {
            serialize_node_list_markdown(&node->as.environment.children, out);
        }
        break;
    }
    }
}

void serialize_ast_markdown(const NodeList *root, FILE *out) {
    for (size_t i = 0; i < root->count; i++) {
        Node *n = root->data[i];
        if (n->type == NODE_ENVIRONMENT && strcmp(n->as.environment.name, "document") == 0) {
            serialize_node_list_markdown(&n->as.environment.children, out);
            fprintf(out, "\n");
            return;
        }
    }

    serialize_node_list_markdown(root, out);
    fprintf(out, "\n");
}
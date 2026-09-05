#include <stdio.h>
#include <stdlib.h>
#include "latex_compiler.h"

static char *read_stdin(void) {
    size_t capacity = 4096;
    size_t length = 0;

    char *buffer = malloc(capacity);
    if (!buffer) {
        perror("Memory allocation failed");
        return NULL;
    }

    while (1) {
        if (length + 1 >= capacity) {
            size_t new_capacity = capacity * 2;
            char *new_buffer = realloc(buffer, new_capacity);

            if (!new_buffer) {
                perror("Memory allocation failed");
                free(buffer);
                return NULL;
            }

            buffer = new_buffer;
            capacity = new_capacity;
        }

        size_t available = capacity - length - 1;
        size_t bytes_read = fread(buffer + length, 1, available, stdin);

        length += bytes_read;

        if (bytes_read < available) {
            if (ferror(stdin)) {
                perror("Failed to read stdin");
                free(buffer);
                return NULL;
            }

            if (feof(stdin)) {
                break;
            }
        }
    }

    buffer[length] = '\0';
    return buffer;
}

int main(void) {
    char *latex = read_stdin();

    if (!latex) {
        return 1;
    }

    int lex_err = 0;
    TokenList token_list = lex(latex, &lex_err);

    free(latex);

    if (lex_err) {
        free_tokens(&token_list);
        return 1;
    }

    Parser parser = new_parser(token_list.tokens, token_list.count);
    NodeList ast_nodes = parse_node_list(&parser, -1, NULL);

    expand_macros(&ast_nodes);

    serialize_ast_markdown(&ast_nodes, stdout);

    for (size_t i = 0; i < ast_nodes.count; i++) {
        free_node(ast_nodes.data[i]);
    }

    free(ast_nodes.data);
    free_tokens(&token_list);

    return 0;
}
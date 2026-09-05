#ifndef LATEX_PARSER_H
#define LATEX_PARSER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    TOKEN_COMMAND,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_TEXT
} TokenType;

typedef struct {
    int line;
    int col;
} Position;

typedef struct {
    TokenType type;
    char *value;
    int start_idx;
    int end_idx;
    Position start_pos;
    Position end_pos;
} Token;

typedef struct {
    Token *tokens;
    size_t count;
    size_t capacity;
} TokenList;

typedef enum {
    NODE_TEXT,
    NODE_COMMAND,
    NODE_GROUP,
    NODE_ENVIRONMENT
} NodeType;

struct Node;

typedef struct {
    struct Node **data;
    size_t count;
    size_t capacity;
} NodeList;

typedef struct {
    char *value;
} TextNode;

typedef struct {
    NodeList children;
} GroupNode;

typedef enum {
    ARG_OPTIONAL,
    ARG_REQUIRED
} ArgType;

typedef struct {
    ArgType type;
    NodeList children;
} CommandArg;

typedef struct {
    char *name;
    CommandArg *args;
    size_t arg_count;
    size_t arg_capacity;
} CommandNode;

typedef struct {
    char *name;
    NodeList opt_args;
    NodeList children;
} EnvironmentNode;

typedef struct Node {
    NodeType type;
    union {
        TextNode text;
        CommandNode command;
        GroupNode group;
        EnvironmentNode environment;
    } as;
} Node;

typedef struct {
    const Token *tokens;
    size_t total_tokens;
    size_t pos;
} Parser;

TokenList lex(const char *file_path, int *out_error);
void free_tokens(TokenList *list);

Parser new_parser(const Token *tokens, size_t count);
NodeList parse_node_list(Parser *p, int stop_token, const char *stop_env_name);
void free_node(Node *node);

void expand_macros(NodeList *root);
void serialize_ast_markdown(const NodeList *root, FILE *out);

#endif
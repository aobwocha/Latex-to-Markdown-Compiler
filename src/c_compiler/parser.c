#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "latex_compiler.h"

static void append_node(NodeList *list, Node *node) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->data = realloc(list->data, list->capacity * sizeof(Node*));
        if (!list->data) {
            perror("Memory allocation failed for NodeList");
            exit(EXIT_FAILURE);
        }
    }
    list->data[list->count++] = node;
}

static void append_command_arg(CommandNode *cmd, ArgType type, NodeList children) {
    if (cmd->arg_count >= cmd->arg_capacity) {
        cmd->arg_capacity = cmd->arg_capacity == 0 ? 2 : cmd->arg_capacity * 2;
        cmd->args = realloc(cmd->args, cmd->arg_capacity * sizeof(CommandArg));
        if (!cmd->args) {
            perror("Memory allocation failed for CommandArg");
            exit(EXIT_FAILURE);
        }
    }
    cmd->args[cmd->arg_count].type = type;
    cmd->args[cmd->arg_count].children = children;
    cmd->arg_count++;
}

void free_node(Node *node) {
    if (!node) return;
    switch (node->type) {
    case NODE_TEXT:
        free(node->as.text.value);
        break;
    case NODE_COMMAND:
        free(node->as.command.name);
        for (size_t i = 0; i < node->as.command.arg_count; i++) {
            for (size_t j = 0; j < node->as.command.args[i].children.count; j++) {
                free_node(node->as.command.args[i].children.data[j]);
            }
            free(node->as.command.args[i].children.data);
        }
        free(node->as.command.args);
        break;
    case NODE_GROUP:
        for (size_t i = 0; i < node->as.group.children.count; i++) {
            free_node(node->as.group.children.data[i]);
        }
        free(node->as.group.children.data);
        break;
    case NODE_ENVIRONMENT:
        free(node->as.environment.name);
        for (size_t i = 0; i < node->as.environment.opt_args.count; i++) {
            free_node(node->as.environment.opt_args.data[i]);
        }
        free(node->as.environment.opt_args.data);
        for (size_t i = 0; i < node->as.environment.children.count; i++) {
            free_node(node->as.environment.children.data[i]);
        }
        free(node->as.environment.children.data);
        break;
    }
    free(node);
}

Parser new_parser(const Token *tokens, size_t count) {
    return (Parser){.tokens = tokens, .total_tokens = count, .pos = 0};
}

static Token peek(Parser *p) {
    if (p->pos >= p->total_tokens) {
        return (Token){.type = TOKEN_TEXT, .value = ""};
    }
    return p->tokens[p->pos];
}

static Token advance(Parser *p) {
    Token tok = peek(p);
    if (p->pos < p->total_tokens) {
        p->pos++;
    }
    return tok;
}

static bool consume(Parser *p, TokenType expected) {
    Token tok = peek(p);
    if (tok.type != expected) {
        return false;
    }
    advance(p);
    return true;
}

static bool is_at_end(Parser *p) {
    return p->pos >= p->total_tokens;
}

static Node *parse_command(Parser *p);
static Node *parse_group(Parser *p);
static Node *parse_text(Parser *p);
static Node *parse_environment(Parser *p);

NodeList parse_node_list(Parser *p, int stop_token, const char *stop_env_name) {
    NodeList nodes = {NULL, 0, 0};
    while (!is_at_end(p)) {
        Token tok = peek(p);

        if (stop_token != -1 && (int)tok.type == stop_token) {
            break;
        }

        if (stop_env_name != NULL && tok.type == TOKEN_COMMAND && strcmp(tok.value, "\\end") == 0) {
            if (p->pos + 2 < p->total_tokens && 
                p->tokens[p->pos + 1].type == TOKEN_LBRACE &&
                p->tokens[p->pos + 2].type == TOKEN_TEXT &&
                strcmp(p->tokens[p->pos + 2].value, stop_env_name) == 0) 
            {
                break;
            }
        }

        if (tok.type == TOKEN_COMMAND) {
            if (strcmp(tok.value, "\\begin") == 0) {
                if (p->pos + 1 < p->total_tokens && p->tokens[p->pos + 1].type == TOKEN_LBRACE) {
                   append_node(&nodes, parse_environment(p));
                   continue;
                }
            } 
            append_node(&nodes, parse_command(p));
        } else if (tok.type == TOKEN_LBRACE) {
            append_node(&nodes, parse_group(p));
        } else {
            append_node(&nodes, parse_text(p));
        }
    }
    return nodes;
}

static Node *parse_environment(Parser *p) {
    consume(p, TOKEN_COMMAND);
    consume(p, TOKEN_LBRACE);
    
    Token name_tok = advance(p); 
    char *env_name = strdup(name_tok.value);
    
    consume(p, TOKEN_RBRACE);
    
    Node *node = malloc(sizeof(Node));
    node->type = NODE_ENVIRONMENT;
    node->as.environment.name = env_name;
    node->as.environment.opt_args = (NodeList){NULL, 0, 0};
    node->as.environment.children = (NodeList){NULL, 0, 0};

    if (peek(p).type == TOKEN_LBRACKET) {
        consume(p, TOKEN_LBRACKET);
        node->as.environment.opt_args = parse_node_list(p, TOKEN_RBRACKET, NULL);
        consume(p, TOKEN_RBRACKET);
    }

    node->as.environment.children = parse_node_list(p, -1, env_name);
    
    consume(p, TOKEN_COMMAND);
    consume(p, TOKEN_LBRACE);
    consume(p, TOKEN_TEXT);
    consume(p, TOKEN_RBRACE);
    
    return node;
}

static Node *parse_command(Parser *p) {
    Token cmd_tok = advance(p);
    Node *node = malloc(sizeof(Node));
    node->type = NODE_COMMAND;
    node->as.command.name = strdup(cmd_tok.value);
    node->as.command.args = NULL;
    node->as.command.arg_count = 0;
    node->as.command.arg_capacity = 0;

    while (!is_at_end(p)) {
        if (peek(p).type == TOKEN_LBRACKET) {
            consume(p, TOKEN_LBRACKET);
            NodeList children = parse_node_list(p, TOKEN_RBRACKET, NULL);
            consume(p, TOKEN_RBRACKET);
            append_command_arg(&node->as.command, ARG_OPTIONAL, children);
        } else if (peek(p).type == TOKEN_LBRACE) {
            consume(p, TOKEN_LBRACE);
            NodeList children = parse_node_list(p, TOKEN_RBRACE, NULL);
            consume(p, TOKEN_RBRACE);
            append_command_arg(&node->as.command, ARG_REQUIRED, children);
        } else {
            break;
        }
    }
    return node;
}

static Node *parse_group(Parser *p) {
    consume(p, TOKEN_LBRACE);
    NodeList children = parse_node_list(p, TOKEN_RBRACE, NULL);
    consume(p, TOKEN_RBRACE);

    Node *node = malloc(sizeof(Node));
    node->type = NODE_GROUP;
    node->as.group.children = children;
    return node;
}

static Node *parse_text(Parser *p) {
    Token tok = advance(p);
    Node *node = malloc(sizeof(Node));
    node->type = NODE_TEXT;
    node->as.text.value = strdup(tok.value);
    return node;
}
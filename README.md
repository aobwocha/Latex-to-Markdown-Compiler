# LaTeX to Markdown Compiler

Converts LaTeX documents into Markdown. A C compiler does the parsing and
translation; a TypeScript/Express microservice wraps it and exposes it over
HTTP.

## How it works

```
HTTP request → Express route → spawn(latex_compiler) → stdout → JSON response
```

The C compiler runs as a single-purpose pipeline:

1. **Lexer** (`lexer.c`) — tokenizes raw LaTeX into commands, braces,
   brackets, and text.
2. **Parser** (`parser.c`) — builds an AST of commands, groups, and
   environments from the token stream.
3. **Macro expander** (`expand.c`) — resolves `\newcommand` definitions
   (0–9 parameters) inside the `document` environment, up to 256
   substitutions per run. Beyond that limit it leaves the macro unexpanded
   and prints a warning, so a cyclic definition can't hang the process.
4. **Serializer** (`serializer.c`) — walks the AST and writes Markdown.

The Node service reads the request body, writes `latexContent` to the
compiler's stdin, and reads the resulting Markdown back from stdout. It does
not read or write any file on disk.

## Supported LaTeX

| LaTeX | Markdown |
| --- | --- |
| `\section{}`, `\ressection{}` | `## ` heading |
| `\subsection{}` | `### ` heading |
| `\textbf{}` | `**bold**` |
| `\textit{}` | `*italic*` |
| `\href{url}{text}` | `[text](url)` |
| `itemize`, `enumerate` | `- ` list (both render as bullets, not numbers) |
| `\\` | hard line break |
| `\hfill` | em dash |
| `\vspace`, `\hspace`, `\centering`, `\label`, and other layout-only commands | dropped |

Only content inside `\begin{document}...\end{document}` is emitted; if no
`document` environment is present, the compiler falls back to serializing
the whole input. Any command not listed above renders generically as
`**[\commandname]** arg1 — arg2` so nothing silently disappears, but the
output won't be idiomatic Markdown.

`\ressection` behaves exactly like `\section` but can't be redefined with
`\newcommand`, so a document can't accidentally break its own section
headings while defining other macros.

## Project structure

```
├── include/
│   └── latex_compiler.h     # shared types for the C compiler
├── src/
│   ├── c_compiler/
│   │   ├── main.c           # reads stdin, runs the pipeline, writes stdout
│   │   ├── lexer.c
│   │   ├── parser.c
│   │   ├── expand.c
│   │   └── serializer.c
│   ├── server.ts            # Express app and /api/parse route
│   ├── compiler.ts          # spawns the compiler binary, handles its process lifecycle
│   └── types.ts             # shared request/response/config types
├── Makefile
├── package.json
├── tsconfig.json
└── run.sh                   # clean, install, build, and run in one step
```

## Prerequisites

- Node.js 18+
- A C compiler toolchain (`gcc`, `make`)

## Build and run

```bash
npm install
npm run build:c    # compiles src/c_compiler/*.c into output/latex_compiler
npm run build:ts   # type-checks and compiles TypeScript into dist/
npm run dev         # runs src/server.ts directly with tsx (no dist/ needed)
```

`npm start` also runs `src/server.ts` directly through `tsx`; the `dist/`
output from `build:ts` isn't what actually runs. Use `build:ts` for type
checking, not as a required build step.

Or run everything with one command:

```bash
./run.sh
```

This removes any previous build output, installs dependencies if
`node_modules` is missing, rebuilds the C binary, confirms
`output/latex_compiler` exists, and starts the dev server.

## Configuration

| Variable | Description | Default |
| --- | --- | --- |
| `PORT` | HTTP server port | `3000` |
| `C_BINARY_PATH` | Path to the compiled binary | `./output/latex_compiler` |
| `C_OUTPUT_PATH` | Reserved for future use; the compiler currently returns Markdown over stdout and never reads or writes this path | `./parser_output.md` |

## API

### `POST /api/parse`

**Request**

```json
{
  "latexContent": "\\section{Introduction}\nHello \\textbf{world}."
}
```

**Response — 200 OK**

```json
{
  "success": true,
  "markdown": "\n## Introduction\n\nHello **world**.\n"
}
```

**Response — 400 Bad Request** (missing `latexContent`) or **500 Internal
Server Error** (compiler exited non-zero or crashed)

```json
{
  "success": false,
  "error": "LaTeX compiler failed (exit code: 1)"
}
```

### Example

```bash
curl -X POST http://localhost:3000/api/parse \
  -H "Content-Type: application/json" \
  -d '{"latexContent": "\\section{Intro}\\textbf{Hello}"}'
```

## Limitations

- `enumerate` and `itemize` both render as bare bullet lists; ordered
  numbering isn't produced.
- Macro expansion is capped at 256 substitutions per document; a definition
  that recurses past that limit is left unexpanded rather than causing an
  infinite loop.
- Commands the serializer doesn't recognize print with their name and
  arguments visible rather than being silently dropped or fully rendered.

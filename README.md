# TeXdown

**TeXdown** is a LaTeX-to-Markdown compiler written in C, with its own lexer, parser, macro expander and serializer, served over HTTP by a TypeScript/Express microservice.

<table>
<tr>
<th>LaTeX in</th>
<th>Markdown out (extra blank lines trimmed)</th>
</tr>
<tr>
<td valign="top">

```latex
\documentclass{article}
\newcommand{\role}[2]{\textbf{#1} \hfill \textit{#2}}
\begin{document}
\section{Experience}
\role{Software Engineer}{2024 -- Present}
\begin{itemize}
\item Built a \href{https://example.com}{LaTeX compiler} in C
\item Wrapped it in an \textbf{Express} API
\end{itemize}
\end{document}
```

</td>
<td valign="top">

```markdown
## Experience

**Software Engineer**  —  *2024 -- Present*

-  Built a [LaTeX compiler](https://example.com) in C

-  Wrapped it in an **Express** API
```

</td>
</tr>
</table>

The custom `\role` macro is expanded, the preamble is dropped, and only the document body is emitted.

## Motivation

I write my official documents in LaTeX, but I edit them more and more with AI assistants. Pasting raw `.tex` into a chat went badly: the model got stuck on layout commands and macros instead of the content. Markdown works far better, but converting by hand every time the document changed was tedious. So I built **TeXdown**, which turns LaTeX into clean Markdown in one request. My documents stay in LaTeX, and my AI tools get a version they can work with.

## Quick Start

### Prerequisites

- Node.js 18+
- A C toolchain (`gcc`, `make`)

### 1. Build and start the service

```bash
# Clean, install dependencies, compile the C binary and start the server on port 3000
./run.sh
```

### 2. Convert a document

```bash
# Send LaTeX, get Markdown back as JSON
curl -X POST http://localhost:3000/api/parse \
  -H "Content-Type: application/json" \
  -d '{"latexContent": "\\section{Intro}\\textbf{Hello}"}'
```

## Usage

### Use the compiler without the server

The C binary works on its own as a Unix filter:

```bash
# Build the binary into output/latex_compiler
make

# Convert a file
./output/latex_compiler < resume.tex > resume.md
```

### API: `POST /api/parse`

**Request body**

```json
{
  "latexContent": "\\section{Introduction}\nHello \\textbf{world}."
}
```

**200 OK**

```json
{
  "success": true,
  "markdown": "\n## Introduction\n\nHello **world**.\n"
}
```

**400 Bad Request** (missing `latexContent`) or **500 Internal Server Error** (the compiler exited non-zero or crashed):

```json
{
  "success": false,
  "error": "LaTeX compiler failed (exit code: 1)"
}
```

Request bodies can be up to **10 MB**.

### Supported LaTeX

| LaTeX | Markdown |
| --- | --- |
| `\section{}`, `\ressection{}` | `## ` heading |
| `\subsection{}` | `### ` heading |
| `\textbf{}` | `**bold**` |
| `\textit{}` | `*italic*` |
| `\href{url}{text}` | `[text](url)` |
| `itemize`, `enumerate` | `- ` list (both render as bullets) |
| `\\` | hard line break |
| `\hfill` | em dash |
| `\vspace`, `\hspace`, `\centering`, `\label` and other layout-only commands | dropped |
| `\newcommand` (0–9 parameters) | expanded inline |

- Only content inside `\begin{document}...\end{document}` is emitted. If there is no `document` environment, the whole input is converted.
- `\ressection` behaves like `\section` but **can't be redefined** with `\newcommand`, so a document can't break its own headings while defining other macros.

### Configuration

Set these environment variables before starting the server:

- `PORT` - HTTP port (default `3000`)
- `C_BINARY_PATH` - path to the compiled binary (default `./output/latex_compiler`)
- `C_OUTPUT_PATH` - reserved for future use; currently unused (default `./parser_output.md`)

### How it works

```
HTTP request → Express route → spawn(latex_compiler) → stdout → JSON response
```

Inside the binary, each stage hands its result to the next:

1. **Lexer** (`lexer.c`) tokenizes raw LaTeX into commands, braces, brackets and text.
2. **Parser** (`parser.c`) builds an AST of commands, groups and environments.
3. **Macro expander** (`expand.c`) resolves `\newcommand` definitions inside the `document` environment.
4. **Serializer** (`serializer.c`) walks the AST and writes Markdown.

```
├── include/latex_compiler.h   # shared types for the C compiler
├── src/
│   ├── c_compiler/            # main.c, lexer.c, parser.c, expand.c, serializer.c
│   ├── server.ts              # Express app and /api/parse route
│   ├── compiler.ts            # spawns the binary and manages its process lifecycle
│   └── types.ts               # request/response/config types
├── Makefile
└── run.sh                     # clean, install, build and run in one step
```

### Known limitations

- `enumerate` renders as a bullet list; ordered numbering isn't produced yet.
- Unrecognized commands appear with their name and arguments visible rather than as idiomatic Markdown.
- Indenting `\item` lines can add stray leading spaces to list items in the output.

## Contributing

### 1. Clone the repo

```bash
git clone https://github.com/aobwocha/Latex-to-Markdown-Compiler.git
cd Latex-to-Markdown-Compiler
```

### 2. Install dependencies

```bash
npm install
```

### 3. Build

```bash
# Compile the C binary into output/latex_compiler
npm run build:c

# Type-check the TypeScript
npm run build:ts
```

### 4. Run the dev server

```bash
# Runs src/server.ts with tsx and reloads on file changes
npm run dev
```

There is no automated test suite yet. To check a change, pipe a `.tex` file through `./output/latex_compiler` or send a request to `/api/parse`.

### 5. Submit a pull request

Fork the repo and open a pull request against the `main` branch.

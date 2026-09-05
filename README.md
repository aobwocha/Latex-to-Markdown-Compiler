# LaTeX to Markdown Microservice

A lightweight Node.js/TypeScript microservice that converts LaTeX content into Markdown by executing an underlying compiler binary via child processes.

## Overview

The service exposes an HTTP POST endpoint that accepts raw LaTeX input and returns transformed Markdown output.

```
[ HTTP Request ] -> [ Express API ] -> [ Native Binary Process ] -> [ Markdown Response ]

```

## Setup

### Prerequisites

- Node.js (v18+)
- A built LaTeX compiler binary available on your system path or output directory

### Environment Variables

| Variable        | Description                 | Default                   |
| --------------- | --------------------------- | ------------------------- |
| `PORT`          | HTTP server port            | `3000`                    |
| `C_BINARY_PATH` | Path to compiler binary     | `./output/latex_compiler` |
| `C_OUTPUT_PATH` | File path for parser output | `./parser_output.md`      |

### Installation

```bash
npm install
npm run build
npm start

```

## API Reference

### Parse LaTeX Content

```http
POST /api/parse
Content-Type: application/json

```

#### Request Body

```json
{
  "latexContent": "\\documentclass{article}\\begin{document}Hello World\\end{document}"
}
```

#### Response Body (`200 OK`)

```json
{
  "success": true,
  "markdown": "# Hello World"
}
```

#### Error Response (`500 Internal Server Error`)

```json
{
  "success": false,
  "error": "LaTeX compiler failed (exit code: 1)"
}
```

## Example Usage

```bash
curl -X POST http://localhost:3000/api/parse \
  -H "Content-Type: application/json" \
  -d '{"latexContent": "\\section*{Introduction}\\nThis is math: $E=mc^2$"}'

```

## Project Structure

```text
├── src/
│   ├── index.ts          # Express server setup and routes
│   ├── latexCompiler.ts  # Child process spawn & lifecycle handling
│   └── types.ts          # Request/response and options interfaces

```

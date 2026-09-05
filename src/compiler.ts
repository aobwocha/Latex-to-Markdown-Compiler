import { spawn } from "child_process";
import type { CompileOptions, CompileResult } from "./types.js";

export async function parseLatexToMarkdown(
  latexContent: string,
  options: CompileOptions,
): Promise<CompileResult> {
  return new Promise((resolve, reject) => {
    const compiler = spawn(options.binaryPath, [], {
      stdio: ["pipe", "pipe", "pipe"],
    });

    let stdout = "";
    let stderr = "";
    let settled = false;

    compiler.stdout.setEncoding("utf8");
    compiler.stderr.setEncoding("utf8");

    compiler.stdout.on("data", (chunk: string) => {
      stdout += chunk;
    });

    compiler.stderr.on("data", (chunk: string) => {
      stderr += chunk;
    });

    compiler.on("error", (error) => {
      if (settled) return;
      settled = true;
      reject(error);
    });

    compiler.on("close", (code, signal) => {
      if (settled) return;
      settled = true;

      if (code === 0) {
        resolve({ markdown: stdout });
        return;
      }

      const diagnostic = stderr.trim();
      const status = signal
        ? `signal: ${signal}`
        : `exit code: ${code ?? "unknown"}`;

      reject(
        new Error(
          diagnostic
            ? `${diagnostic}\nCompiler ${status}`
            : `LaTeX compiler failed (${status})`,
        ),
      );
    });

    compiler.stdin.on("error", (error) => {
      if (settled) return;
      settled = true;
      reject(error);
    });

    compiler.stdin.end(latexContent, "utf8");
  });
}

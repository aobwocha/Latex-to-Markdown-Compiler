import express, { Request, Response } from "express";
import path from "path";
import { parseLatexToMarkdown } from "./compiler.js";
import type {
  CompileOptions,
  ParseApiRequest,
  ParseApiResponse,
} from "./types.js";

const app = express();
const PORT = process.env.PORT ?? 3000;

const CONFIG: CompileOptions = {
  binaryPath:
    process.env.C_BINARY_PATH ?? path.resolve("output/latex_compiler"),
  outputPath: process.env.C_OUTPUT_PATH ?? path.resolve("parser_output.md"),
};

app.use(express.json({ limit: "10mb" }));

app.post(
  "/api/parse",
  async (
    req: Request<{}, {}, ParseApiRequest>,
    res: Response<ParseApiResponse>,
  ) => {
    try {
      const { latexContent } = req.body;

      if (!latexContent) {
        return res
          .status(400)
          .json({ success: false, error: "latexContent is required" });
      }

      const { markdown } = await parseLatexToMarkdown(latexContent, CONFIG);
      return res.json({ success: true, markdown });
    } catch (error: any) {
      console.error("LaTeX compilation error:", error);
      return res.status(500).json({
        success: false,
        error: error.message ?? "Error executing LaTeX compiler service",
      });
    }
  },
);

app.listen(PORT, () => {
  console.log(`LaTeX Parser Microservice listening on port ${PORT}`);
});

export interface CompileOptions {
  binaryPath: string;
  outputPath: string;
}

export interface CompileResult {
  markdown: string;
}

export interface ParseApiRequest {
  latexContent: string;
}

export interface ParseApiResponse {
  success: boolean;
  markdown?: string;
  error?: string;
}

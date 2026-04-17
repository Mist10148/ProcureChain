# AI Summarizer Module

This module contains the Python runner used by ProcureChain to generate AI summaries for selected procurement documents.

Last updated: 2026-04-17

## Folder Layout

- scripts/run_gemini_summary.py: Gemini runner and cache updater
- workspace/input: copied/prepared input files before summary generation
- workspace/output: generated summary text files

## Runtime Inputs

- Environment variable: GEMINI_API_KEY
- Requested model label from C++ flow: `gemini-3.0-flash` (with compatible API fallback candidates)
- Python dependencies from `ai_summarizer/requirements.txt`

## Setup

From the project root:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r ai_summarizer\requirements.txt
```

Set `GEMINI_API_KEY` using one option:

```powershell
$env:GEMINI_API_KEY = "your_api_key_here"
```

Or use `.env` in project root:

```powershell
Copy-Item .env.example .env
# edit .env and set GEMINI_API_KEY
```

The runner explicitly loads `.env` when `python-dotenv` is installed.

## Relationship to data/uploads and data/verify

- `data/uploads` is used by admin upload workflows (document ingestion), not by summarizer generation.
- `data/verify` is used by citizen hash-verification workflows, not by summarizer generation.
- Summarizer input/output files are under `ai_summarizer/workspace/input` and `ai_summarizer/workspace/output`.

## Cache File

The C++ app reads and writes metadata in data/summarizer.txt.

# AI Summarizer Module

This module contains the Python runner used by ProcureChain to generate AI summaries for selected procurement documents.

## Folder Layout

- scripts/run_gemini_summary.py: Gemini runner and cache updater
- workspace/input: copied/prepared input files before summary generation
- workspace/output: generated summary text files

## Runtime Inputs

- Environment variable: GEMINI_API_KEY
- Python packages:
  - google-generativeai
  - pypdf
  - python-docx

## Cache File

The C++ app reads and writes metadata in data/summarizer.txt.

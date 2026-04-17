#!/usr/bin/env python3
import argparse
import csv
import datetime
import os
import pathlib
import sys

HEADER = "docID|updatedAt|status|model|summaryFile|sourceFile|error"


def sanitize_field(value: str) -> str:
    return value.replace("|", "/").replace("\n", " ").replace("\r", " ").strip()


def now_text() -> str:
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def read_text_from_input(input_path: pathlib.Path) -> str:
    suffix = input_path.suffix.lower()

    if suffix in {".txt", ".md", ".log"}:
        return input_path.read_text(encoding="utf-8", errors="ignore")

    if suffix == ".csv":
        lines = []
        with input_path.open("r", encoding="utf-8", errors="ignore", newline="") as f:
            reader = csv.reader(f)
            for row in reader:
                lines.append(", ".join(row))
        return "\n".join(lines)

    if suffix == ".pdf":
        try:
            from pypdf import PdfReader
        except Exception as exc:
            raise RuntimeError("PDF support requires pypdf package") from exc

        reader = PdfReader(str(input_path))
        pages = []
        for page in reader.pages:
            pages.append(page.extract_text() or "")
        return "\n".join(pages)

    if suffix == ".docx":
        try:
            import docx
        except Exception as exc:
            raise RuntimeError("DOCX support requires python-docx package") from exc

        document = docx.Document(str(input_path))
        return "\n".join(p.text for p in document.paragraphs)

    raise RuntimeError(f"Unsupported input extension: {suffix}")


def update_cache(cache_path: pathlib.Path, row: dict) -> None:
    rows = []
    if cache_path.exists():
        for line in cache_path.read_text(encoding="utf-8", errors="ignore").splitlines():
            if not line.strip() or line.startswith("docID|"):
                continue
            tokens = line.split("|")
            if len(tokens) < 7:
                continue
            rows.append(
                {
                    "docID": tokens[0],
                    "updatedAt": tokens[1],
                    "status": tokens[2],
                    "model": tokens[3],
                    "summaryFile": tokens[4],
                    "sourceFile": tokens[5],
                    "error": tokens[6],
                }
            )

    replaced = False
    for i, existing in enumerate(rows):
        if existing["docID"] == row["docID"]:
            rows[i] = row
            replaced = True
            break

    if not replaced:
        rows.append(row)

    out_lines = [HEADER]
    for item in rows:
        out_lines.append(
            "|".join(
                [
                    sanitize_field(item["docID"]),
                    sanitize_field(item["updatedAt"]),
                    sanitize_field(item["status"]),
                    sanitize_field(item["model"]),
                    sanitize_field(item["summaryFile"]),
                    sanitize_field(item["sourceFile"]),
                    sanitize_field(item["error"]),
                ]
            )
        )

    cache_path.parent.mkdir(parents=True, exist_ok=True)
    cache_path.write_text("\n".join(out_lines) + "\n", encoding="utf-8")


def summarize_with_gemini(text: str, model_name: str) -> str:
    api_key = os.environ.get("GEMINI_API_KEY", "").strip()
    if not api_key:
        raise RuntimeError("GEMINI_API_KEY is not set")

    try:
        import google.generativeai as genai
    except Exception as exc:
        raise RuntimeError("google-generativeai package is required") from exc

    genai.configure(api_key=api_key)
    model = genai.GenerativeModel(model_name)

    prompt = (
        "You are summarizing a municipal procurement document for transparency viewers. "
        "Provide a concise summary with sections: Purpose, Key Scope, Budget/Cost Clues, "
        "Timeline Clues, Risks/Compliance Notes, and a 1-sentence Plain-Language Takeaway.\n\n"
        f"Document content:\n{text[:120000]}"
    )

    response = model.generate_content(prompt)
    output = getattr(response, "text", "") or ""
    output = output.strip()
    if not output:
        raise RuntimeError("Gemini returned empty summary")
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--doc-id", required=True)
    parser.add_argument("--input-path", required=True)
    parser.add_argument("--summary-path", required=True)
    parser.add_argument("--cache-path", required=True)
    parser.add_argument("--model", default="gemini-1.5-flash")
    args = parser.parse_args()

    doc_id = args.doc_id.strip()
    input_path = pathlib.Path(args.input_path)
    summary_path = pathlib.Path(args.summary_path)
    cache_path = pathlib.Path(args.cache_path)

    try:
        if not input_path.exists():
            raise RuntimeError(f"Input file not found: {input_path}")

        content = read_text_from_input(input_path)
        if not content.strip():
            raise RuntimeError("Input document has no readable text")

        summary = summarize_with_gemini(content, args.model)

        summary_path.parent.mkdir(parents=True, exist_ok=True)
        summary_path.write_text(summary, encoding="utf-8")

        update_cache(
            cache_path,
            {
                "docID": doc_id,
                "updatedAt": now_text(),
                "status": "ok",
                "model": args.model,
                "summaryFile": str(summary_path),
                "sourceFile": str(input_path),
                "error": "",
            },
        )
        return 0

    except Exception as exc:
        update_cache(
            cache_path,
            {
                "docID": doc_id,
                "updatedAt": now_text(),
                "status": "error",
                "model": args.model,
                "summaryFile": str(summary_path),
                "sourceFile": str(input_path),
                "error": str(exc),
            },
        )
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

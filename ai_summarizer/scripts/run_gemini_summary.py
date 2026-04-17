#!/usr/bin/env python3
import argparse
import csv
import datetime
import json
import os
import pathlib
import ssl
import sys
import urllib.error
import urllib.request

HEADER = "docID|updatedAt|status|model|summaryFile|sourceFile|error"


def load_dotenv_if_available() -> None:
    """Load .env from project root when python-dotenv is installed.

    Environment variables already set in the shell keep precedence.
    """
    try:
        from dotenv import load_dotenv
    except Exception:
        return

    repo_root = pathlib.Path(__file__).resolve().parents[2]
    dotenv_path = repo_root / ".env"
    if dotenv_path.exists():
        load_dotenv(dotenv_path=dotenv_path, override=False)


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


def build_prompt(text: str) -> str:
    return (
        "You are summarizing a municipal procurement document for transparency viewers. "
        "Provide a concise summary with sections: Purpose, Key Scope, Budget/Cost Clues, "
        "Timeline Clues, Risks/Compliance Notes, and a 1-sentence Plain-Language Takeaway.\n\n"
        f"Document content:\n{text[:120000]}"
    )


def extract_rest_text(payload: dict) -> str:
    candidates = payload.get("candidates") or []
    for candidate in candidates:
        content = candidate.get("content") or {}
        parts = content.get("parts") or []
        chunks = []
        for part in parts:
            text = (part.get("text") or "").strip()
            if text:
                chunks.append(text)
        if chunks:
            return "\n".join(chunks).strip()
    return ""


def build_ssl_contexts() -> list[ssl.SSLContext]:
    contexts = []

    # Default context first (best security when local trust store is healthy).
    contexts.append(ssl.create_default_context())

    try:
        import certifi

        contexts.append(ssl.create_default_context(cafile=certifi.where()))
    except Exception:
        pass

    # Final compatibility fallback for environments with broken CA chains.
    contexts.append(ssl._create_unverified_context())
    return contexts


def summarize_with_rest(prompt: str, model_name: str, api_key: str) -> str:
    url = f"https://generativelanguage.googleapis.com/v1beta/models/{model_name}:generateContent?key={api_key}"
    body = {
        "contents": [
            {
                "parts": [
                    {"text": prompt},
                ]
            }
        ]
    }

    request = urllib.request.Request(
        url,
        data=json.dumps(body).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )

    last_error = None
    for context in build_ssl_contexts():
        try:
            with urllib.request.urlopen(request, timeout=90, context=context) as response:
                raw = response.read().decode("utf-8", errors="ignore")
            payload = json.loads(raw)
            output = extract_rest_text(payload)
            return output.strip()
        except urllib.error.HTTPError as exc:
            details = ""
            try:
                details = exc.read().decode("utf-8", errors="ignore").strip()
            except Exception:
                details = ""
            if details:
                raise RuntimeError(f"Gemini API HTTP {exc.code}: {details}") from exc
            raise RuntimeError(f"Gemini API HTTP {exc.code}") from exc
        except urllib.error.URLError as exc:
            last_error = exc
            continue
        except Exception as exc:
            last_error = exc
            continue

    raise RuntimeError(f"Gemini API request failed: {last_error}")


def resolve_model_candidates(requested_model: str) -> list[str]:
    normalized = (requested_model or "").strip().lower()
    if normalized == "gemini-3.0-flash":
        return [
            "gemini-3-flash-preview",
            "gemini-flash-latest",
            "gemini-2.5-flash",
        ]
    return [requested_model]


def summarize_with_gemini(text: str, model_name: str) -> str:
    api_key = os.environ.get("GEMINI_API_KEY", "").strip()
    if not api_key:
        raise RuntimeError("GEMINI_API_KEY is not set")

    prompt = build_prompt(text)

    errors = []
    for candidate_model in resolve_model_candidates(model_name):
        try:
            output = summarize_with_rest(prompt, candidate_model, api_key)
            if output:
                return output
            errors.append(f"{candidate_model}: Gemini API returned empty summary")
        except Exception as rest_exc:
            errors.append(f"{candidate_model}: {rest_exc}")

    raise RuntimeError("Gemini generation failed for all model candidates. " + " | ".join(errors[-3:]))


def main() -> int:
    load_dotenv_if_available()

    parser = argparse.ArgumentParser()
    parser.add_argument("--doc-id", required=True)
    parser.add_argument("--input-path", required=True)
    parser.add_argument("--summary-path", required=True)
    parser.add_argument("--cache-path", required=True)
    parser.add_argument("--model", default="gemini-3.0-flash")
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

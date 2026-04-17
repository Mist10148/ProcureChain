#include "../include/summarizer.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
const std::string CACHE_FILE_PRIMARY = "data/summarizer.txt";
const std::string CACHE_FILE_FALLBACK = "../data/summarizer.txt";

const std::string WORK_INPUT_PRIMARY = "ai_summarizer/workspace/input";
const std::string WORK_INPUT_FALLBACK = "../ai_summarizer/workspace/input";
const std::string WORK_OUTPUT_PRIMARY = "ai_summarizer/workspace/output";
const std::string WORK_OUTPUT_FALLBACK = "../ai_summarizer/workspace/output";

const std::string SCRIPT_PRIMARY = "ai_summarizer/scripts/run_gemini_summary.py";
const std::string SCRIPT_FALLBACK = "../ai_summarizer/scripts/run_gemini_summary.py";

const std::string PYTHON_PRIMARY = ".venv/Scripts/python.exe";
const std::string PYTHON_FALLBACK = "../.venv/Scripts/python.exe";

struct SummaryCacheRow {
    std::string docId;
    std::string updatedAt;
    std::string status;
    std::string model;
    std::string summaryFile;
    std::string sourceFile;
    std::string error;
};

std::vector<std::string> splitPipe(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream parser(line);
    std::string token;
    while (std::getline(parser, token, '|')) {
        out.push_back(token);
    }
    return out;
}

bool openInputFileWithFallback(std::ifstream& file, const std::string& primaryPath, const std::string& fallbackPath) {
    file.open(primaryPath);
    if (file.is_open()) {
        return true;
    }

    file.clear();
    file.open(fallbackPath);
    return file.is_open();
}

std::string resolveCachePath() {
    std::ifstream primary(CACHE_FILE_PRIMARY);
    if (primary.is_open()) {
        return CACHE_FILE_PRIMARY;
    }

    std::ifstream fallback(CACHE_FILE_FALLBACK);
    if (fallback.is_open()) {
        return CACHE_FILE_FALLBACK;
    }

    std::ofstream createPrimary(CACHE_FILE_PRIMARY);
    if (createPrimary.is_open()) {
        createPrimary << "docID|updatedAt|status|model|summaryFile|sourceFile|error\n";
        createPrimary.flush();
        return CACHE_FILE_PRIMARY;
    }

    std::ofstream createFallback(CACHE_FILE_FALLBACK);
    if (createFallback.is_open()) {
        createFallback << "docID|updatedAt|status|model|summaryFile|sourceFile|error\n";
        createFallback.flush();
        return CACHE_FILE_FALLBACK;
    }

    return "";
}

std::string resolveDirectoryPath(const std::string& primaryPath, const std::string& fallbackPath) {
    std::error_code ec;
    std::filesystem::create_directories(primaryPath, ec);
    if (!ec) {
        return primaryPath;
    }

    ec.clear();
    std::filesystem::create_directories(fallbackPath, ec);
    if (!ec) {
        return fallbackPath;
    }

    return "";
}

std::string resolveScriptPath() {
    if (std::filesystem::exists(SCRIPT_PRIMARY)) {
        return SCRIPT_PRIMARY;
    }
    if (std::filesystem::exists(SCRIPT_FALLBACK)) {
        return SCRIPT_FALLBACK;
    }
    return "";
}

std::string resolvePythonCommand() {
    if (std::filesystem::exists(PYTHON_PRIMARY)) {
        return PYTHON_PRIMARY;
    }
    if (std::filesystem::exists(PYTHON_FALLBACK)) {
        return PYTHON_FALLBACK;
    }
    return "python";
}

std::string sanitizeFileToken(std::string value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        const bool safe = (std::isalnum(c) != 0) || value[i] == '_' || value[i] == '-';
        if (!safe) {
            value[i] = '_';
        }
    }
    if (value.empty()) {
        value = "doc";
    }
    return value;
}

std::string readFileContent(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

std::string buildSummaryOutputPath(const std::string& outputDir, const std::string& docId) {
    return (std::filesystem::path(outputDir) / (sanitizeFileToken(docId) + "_summary.txt")).string();
}

bool loadSummaryCacheRows(const std::string& cachePath, std::vector<SummaryCacheRow>& rows) {
    std::ifstream file(cachePath);
    if (!file.is_open()) {
        return false;
    }

    rows.clear();
    std::string line;
    bool firstLine = true;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            if (line.find("docID|") == 0) {
                continue;
            }
        }

        const std::vector<std::string> tokens = splitPipe(line);
        if (tokens.size() < 3) {
            continue;
        }

        SummaryCacheRow row;
        row.docId = tokens[0];
        row.updatedAt = tokens.size() > 1 ? tokens[1] : "";
        row.status = tokens.size() > 2 ? tokens[2] : "";
        row.model = tokens.size() > 3 ? tokens[3] : "";
        row.summaryFile = tokens.size() > 4 ? tokens[4] : "";
        row.sourceFile = tokens.size() > 5 ? tokens[5] : "";
        row.error = tokens.size() > 6 ? tokens[6] : "";
        rows.push_back(row);
    }

    return true;
}

bool findSummaryCacheRow(const std::string& cachePath, const std::string& docId, SummaryCacheRow& row) {
    std::vector<SummaryCacheRow> rows;
    if (!loadSummaryCacheRows(cachePath, rows)) {
        return false;
    }

    for (std::size_t i = rows.size(); i > 0; --i) {
        if (rows[i - 1].docId == docId) {
            row = rows[i - 1];
            return true;
        }
    }

    return false;
}

DocumentSummaryResult buildCacheResult(const SummaryCacheRow& row) {
    DocumentSummaryResult result;
    result.success = (row.status == "ok");
    result.fromCache = true;
    result.hasCachedSummary = false;
    result.docId = row.docId;
    result.updatedAt = row.updatedAt;
    result.model = row.model;
    result.summary = "";
    result.message = row.error;

    if (!row.summaryFile.empty()) {
        result.summary = readFileContent(row.summaryFile);
        if (!result.summary.empty()) {
            result.hasCachedSummary = true;
        }
    }

    if (result.success && result.summary.empty()) {
        result.success = false;
        result.message = "Summary cache entry exists but summary file is missing.";
    }

    if (result.success && result.message.empty()) {
        result.message = "Loaded summary from cache.";
    }

    return result;
}

bool writeFallbackInputText(const std::string& inputDir,
                            const std::string& docId,
                            const std::string& fallbackText,
                            std::string& outPath) {
    outPath = (std::filesystem::path(inputDir) / (sanitizeFileToken(docId) + "_metadata_input.txt")).string();
    std::ofstream out(outPath, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << fallbackText;
    out.flush();
    return true;
}

bool copyInputFileToWorkspace(const std::string& sourcePath,
                              const std::string& inputDir,
                              const std::string& docId,
                              std::string& outPath) {
    std::error_code ec;
    std::filesystem::path source(sourcePath);
    if (sourcePath.empty() || !std::filesystem::exists(source, ec) || ec || !std::filesystem::is_regular_file(source, ec)) {
        return false;
    }

    const std::string extension = source.extension().string().empty() ? ".txt" : source.extension().string();
    std::filesystem::path target = std::filesystem::path(inputDir) / (sanitizeFileToken(docId) + "_input" + extension);

    std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        return false;
    }

    outPath = target.string();
    return true;
}
}

DocumentSummaryResult getDocumentSummary(const std::string& docId) {
    DocumentSummaryResult result;
    result.success = false;
    result.fromCache = false;
    result.hasCachedSummary = false;
    result.docId = docId;
    result.updatedAt = "";
    result.model = "";
    result.summary = "";
    result.message = "No summary has been generated yet.";

    const std::string cachePath = resolveCachePath();
    if (cachePath.empty()) {
        result.message = "Unable to access summary cache file.";
        return result;
    }

    SummaryCacheRow row;
    if (!findSummaryCacheRow(cachePath, docId, row)) {
        return result;
    }

    return buildCacheResult(row);
}

DocumentSummaryResult generateDocumentSummary(const std::string& docId,
                                              const std::string& sourceFilePath,
                                              const std::string& fallbackText,
                                              bool forceRefresh) {
    DocumentSummaryResult cached = getDocumentSummary(docId);
    if (!forceRefresh && cached.success && cached.hasCachedSummary) {
        cached.message = "Using cached AI summary. Choose refresh to regenerate.";
        return cached;
    }

    const std::string inputDir = resolveDirectoryPath(WORK_INPUT_PRIMARY, WORK_INPUT_FALLBACK);
    const std::string outputDir = resolveDirectoryPath(WORK_OUTPUT_PRIMARY, WORK_OUTPUT_FALLBACK);
    const std::string cachePath = resolveCachePath();
    const std::string scriptPath = resolveScriptPath();

    if (inputDir.empty() || outputDir.empty() || cachePath.empty() || scriptPath.empty()) {
        DocumentSummaryResult fail = cached;
        fail.success = false;
        fail.fromCache = false;
        fail.docId = docId;
        fail.message = "Unable to prepare AI summarizer runtime paths.";
        return fail;
    }

    std::string inputPath;
    const bool copied = copyInputFileToWorkspace(sourceFilePath, inputDir, docId, inputPath);
    if (!copied) {
        if (!writeFallbackInputText(inputDir, docId, fallbackText, inputPath)) {
            DocumentSummaryResult fail = cached;
            fail.success = false;
            fail.fromCache = false;
            fail.docId = docId;
            fail.message = "Unable to prepare summary input file.";
            return fail;
        }
    }

    const std::string outputPath = buildSummaryOutputPath(outputDir, docId);
        const std::string pythonCommand = resolvePythonCommand();

    std::ostringstream command;
        command << "\"" << pythonCommand << "\""
            << " \"" << scriptPath << "\""
            << " --doc-id \"" << docId << "\""
            << " --input-path \"" << inputPath << "\""
            << " --summary-path \"" << outputPath << "\""
            << " --cache-path \"" << cachePath << "\""
            << " --model \"gemini-3.0-flash\"";

    const int code = std::system(command.str().c_str());

    SummaryCacheRow row;
    if (findSummaryCacheRow(cachePath, docId, row)) {
        DocumentSummaryResult latest = buildCacheResult(row);
        latest.fromCache = false;
        latest.docId = docId;

        if (latest.success) {
            latest.message = "AI summary generated successfully.";
            return latest;
        }

        if (cached.success && cached.hasCachedSummary) {
            latest.hasCachedSummary = true;
            latest.summary = cached.summary;
            latest.message = "AI summary generation failed; showing last cached summary.";
        }

        return latest;
    }

    DocumentSummaryResult fail = cached;
    fail.success = false;
    fail.fromCache = false;
    fail.docId = docId;
    fail.message = "AI summary generation failed (runner exit code: " + std::to_string(code) + ").";
    return fail;
}

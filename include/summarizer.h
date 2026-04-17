#ifndef SUMMARIZER_H
#define SUMMARIZER_H

#include <string>

struct DocumentSummaryResult {
    bool success;
    bool fromCache;
    bool hasCachedSummary;
    std::string docId;
    std::string updatedAt;
    std::string model;
    std::string summary;
    std::string message;
};

DocumentSummaryResult getDocumentSummary(const std::string& docId);
DocumentSummaryResult generateDocumentSummary(const std::string& docId,
                                              const std::string& sourceFilePath,
                                              const std::string& fallbackText,
                                              bool forceRefresh);

#endif
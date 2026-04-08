#ifndef DOCUMENTS_H
#define DOCUMENTS_H

#include <string>
#include "auth.h"

struct Document {
    std::string docId;
    std::string title;
    std::string category;
    std::string department;
    std::string dateUploaded;
    std::string uploader;
    std::string status;
    std::string hashValue;
};

std::string generateNextDocumentId();
void ensureSampleDocumentsPresent();
void showPublishedDocuments(const std::string& actor);
void uploadDocumentAsAdmin(const Admin& admin);
void viewAllDocumentsForAdmin(const Admin& admin);
void searchDocumentByIdForAdmin(const Admin& admin);
bool updateDocumentStatusBySystem(const std::string& docId, const std::string& newStatus);
void updateDocumentStatusForAdmin(const Admin& admin);

#endif

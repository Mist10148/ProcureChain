#ifndef DOCUMENTS_H
#define DOCUMENTS_H

#include <string>
#include "auth.h"

struct Document {
    std::string docId;
    std::string title;
    std::string category;
    std::string tags;
    std::string description;
    std::string department;
    std::string dateUploaded;
    std::string uploader;
    std::string status;
    std::string hashValue;
    std::string fileName;
    std::string fileType;
    std::string filePath;
    long long fileSizeBytes;
    std::string budgetCategory;
    double amount;
    int versionNumber;
    std::string previousDocId;
};

std::string generateNextDocumentId();
void ensureSampleDocumentsPresent();
void showPublishedDocuments(const std::string& actor);
void searchPublishedDocumentForCitizen(const std::string& actor);
void viewPublishedDocumentDetailForCitizenById(const std::string& docId, const std::string& actor);
void uploadDocumentAsAdmin(const Admin& admin);
void viewAllDocumentsForAdmin(const Admin& admin);
void searchDocumentByIdForAdmin(const Admin& admin);
void filterDocumentsForAdmin(const Admin& admin);
void exportDocumentsToTxt(const Admin& admin);
bool updateDocumentStatusBySystem(const std::string& docId, const std::string& newStatus);
bool updateDocumentStatusBySystem(const std::string& docId,
                                  const std::string& newStatus,
                                  const std::string& actorUsername,
                                  const std::string& reasonNote);
void updateDocumentStatusForAdmin(const Admin& admin);

#endif

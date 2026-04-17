#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <string>
#include "auth.h"

void logTamperAlert(const std::string& severity,
					const std::string& source,
					const std::string& targetId,
					const std::string& detail,
					const std::string& actor,
					const std::string& visibility);
void showAdminNotificationInbox(const Admin& admin);
void showCitizenNotificationInbox(const User& citizen);

#endif

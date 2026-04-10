#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <string>
#include "auth.h"

void showAdminNotificationInbox(const Admin& admin);
void showCitizenNotificationInbox(const User& citizen);

#endif

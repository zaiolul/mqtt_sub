#ifndef MAIL_UTILS_H
#define MAIL_UTILS_H
#include "events.h"
/*sends MESSAGE from SENDER_MAIL to all RECEIVERS*/
int send_email(char *sender_mail, char receivers[][MAX_STR_LEN], char *message);

#endif
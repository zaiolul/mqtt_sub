#ifndef UCI_UTILS_H
#define UCI_UTILS_H
#include "events.h"
#include "mosquitto.h"

int get_topic_events(struct event *events, char *topic);
int get_sender_data(struct sender *sender, char *sender_mail);
int get_all_topics(struct topic *topics);
int uci_start();
int uci_end();

#endif
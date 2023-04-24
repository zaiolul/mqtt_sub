#ifndef EVENTS_H
#define EVENTS_H

#define EVENT_CAP 128
#define MAX_RECEIVERS 16
#define MAX_SENDERS 16
#define MAX_STR_LEN 256
#define MAX_VAL_LEN 30

struct event{
    char topic[MAX_STR_LEN];
    char key[MAX_STR_LEN];
    int comparison;
    int value_type;
    char value[MAX_VAL_LEN];
    char sender[MAX_STR_LEN];
    char receivers[MAX_RECEIVERS][MAX_STR_LEN];
};

struct sender{
    char email[MAX_STR_LEN];
    char user[MAX_STR_LEN];
    char password[MAX_STR_LEN];
    char smtp_ip[MAX_STR_LEN];
    int port;
    int credentials;
};

int compare_values(char* value1, char* value2, int type, int comparison);
int check_events(char *topic, char *payload);
int print_event(struct event event);

#endif
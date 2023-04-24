#ifndef MQTT_UTILS_H
#define MQTT_UTILS_H
#include <argp.h>
#include "events.h"
#include <mosquitto.h>

#define TOPIC_CAP 16
struct topic{
    char topic[MAX_STR_LEN];
    int qos;
};

struct arguments{
    int daemon;
    int port;
    char *host;
    char *user;
    char *password;
    /*pre-shared key*/
    char *psk;
    char *identity;
    /*server ca certificate*/
    char *cafile;
    /*client*/
    char *certfile; //client cert
    char *keyfile; //client pk

};

static char doc[] = "mqtt subscriber";

/*argp options struct*/
static struct argp_option options[] = {
    {"host", 'h', "[IP]", 0, 0 },
    {"port", 'p', "[PORT]", 0, 0 },
    {"user", 'u', "[USER]", 0, 0 },
    {"password", 'P', "[PASSWORD]", 0},
    {"daemon", 'd', 0, 0},
    {"cafile", 'c', "[PATH]", 0},
    {"certfile", 'C', "[PATH]", 0},
    {"keyfile", 'K', "[PATH]", 0},
    {"psk", 'S', "[KEY]", 0},
    {"identity", 'I', "[NAME]", 0},
    {0}
};

enum{
    JSON_ERR = -1,
    EVENT_CONFIG_ERR = -2,
    TOPIC_CONFIG_ERR = -3,
    SENDER_DATA_ERR = -4,
    FIELD_NOT_FOUND = -5,
};
/*option parsing function*/
error_t parse_opt (int key, char *arg, struct argp_state *state);

int mqtt_main(struct arguments args);


#endif
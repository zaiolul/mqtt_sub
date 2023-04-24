#include <stdlib.h>
#include <argp.h>
#include "mqtt_utils.h"

static struct argp argp = { options, parse_opt, 0, doc };

int main(int argc, char* argv[])
{
    struct arguments args = {
        .daemon = 0,
        .host = NULL,
        .port = 1883,
        .password = NULL,
        .user = NULL,
        .cafile = NULL,
        .psk = NULL,
    };

    argp_parse (&argp, argc, argv, 0, 0, &args);
    
    return mqtt_main(args);
}
#include <stdlib.h>
#include <argp.h>
#include "mqtt_utils.h"
//TODO: persikelti usergroups config i sito pc /etc/config, pasiziuret memory leak su event trigger kazkas susije
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
    
    if(argp_parse (&argp, argc, argv, ARGP_NO_EXIT, 0, &args) != 0)
        return -1;
    
    return mqtt_main(args);
  
}
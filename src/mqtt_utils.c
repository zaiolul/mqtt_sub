#include <mosquitto.h>
#include <argp.h>
#include "mqtt_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include "uci.h"
#include <string.h>
#include "uci_utils.h"
#include <syslog.h>
#include <json-c/json.h>
#include "events.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "logger.h"

struct mosquitto *mosq;
int run = 1;
/*argument parser func*/
error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    struct arguments *arguments = state->input;
   
    
    switch (key){
    case 'd':
        arguments->daemon = 1;
        break;

    case 'u':
        arguments->user = arg;
        break;

    case 'P':
        arguments->password = arg;
        break;


    case 'h':
        arguments->host = arg;
        break;
    
    case 'p':
        arguments->port = atoi(arg);
        break;

    case 'c':
        arguments->cafile = arg;
        break;
    case 'C':
        arguments->certfile = arg;
        break;
    case 'K':
        arguments->keyfile = arg;
        break;
    case 'S':
        arguments->psk = arg;
        break;
    case 'I':
        arguments->identity = arg;
        break;
       

    case ARGP_KEY_END:
    {
        if (arguments->host == NULL){
            argp_failure(state, 1, 0, "host must be provided");
        }

        if(arguments->psk != NULL && arguments->cafile != NULL){
            argp_failure(state, 1, 0, "can't use both preshared key and certificates");
        }
        break;
    }
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}
/*makes the program a daemon*/
int daemonize()
{
    int pid;

    if((pid = fork()) > 0){
        exit(0);
    } else if(pid == -1){
        return -1;
    }
    
    if(setsid() == -1)               
        return -1;

    if((pid = fork()) > 0){
        exit(0);
    } else if(pid == -1){
        return -1;
    }

    umask(0);
    chdir("/");
    
    close(STDIN_FILENO);

    int fd = open("/dev/null", O_RDWR);
    if(fd != STDIN_FILENO)
      return -1;
    if(dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
      return -2;
    if(dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
      return -3;

    return 0;
}

/*Writes the topic and received message to a file*/
int log_to_file(char *topic, char *message, char* file)
{
    time_t now;
    time(&now);
    struct tm *tm_now = localtime(&now);
    char buff[100];
    strftime(buff, sizeof(buff), "%Y-%m-%d, %H:%M:%S", tm_now) ;
    FILE *fs = fopen(file, "a+");
    fprintf(fs, "---------------------\n");
    fprintf(fs, "Time: %s\nTopic: %s\nMessage: %s\n", buff, topic, message);
    fclose(fs);
    return 0;
}
/*Subscribres to TOPIC*/
int subscribe_topic(struct topic topic)
{
    int ret = mosquitto_subscribe(mosq, NULL, topic.topic, topic.qos);
    if(ret != MOSQ_ERR_SUCCESS){
        syslog(LOG_ERR, "Failed subscription: %s, %s\n",topic.topic,  mosquitto_strerror(ret));
    }
    return ret;
}

/*Finds all topics in config and tries to subsribe to them*/
int subscribe_all_topics()
{
    struct topic topics[TOPIC_CAP] = {0};
    int topic_count = get_all_topics(topics);
    int subscribed_count = 0;
    if(topic_count < 0){
        return topic_count;
    }
    for(int i = 0; i < topic_count; i ++)
    {
        printf("%s %d\n", topics[i].topic, topics[i].qos);
        if(subscribe_topic(topics[i]) == MOSQ_ERR_SUCCESS)
            subscribed_count ++;
    } 
    return subscribed_count;
}

/*Callback method for messages
Logs received data to file, checks event triggers*/
void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
{
    time_t now;
    time(&now);
    struct tm *tm_now = localtime(&now);
    char buff[100];
    strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", tm_now) ;
    //log_to_file(msg->topic, (char*)msg->payload, LOG_FILE);
    sqlite_db_insert_message(buff, msg->topic, (char *)msg->payload);

    int ret = check_events(msg->topic, (char*)msg->payload);
    switch(ret){
        case JSON_ERR:
            break;
        case EVENT_CONFIG_ERR:
            syslog(LOG_ERR, "Error in events config, exiting");
            run = 0;
        default:
            break;
    }
}

/*Callback method for sucessful connection
Subscribes to topic provided in config*/
void on_connect(struct mosquitto *mosq, void *obj, int reason_code)
{
    syslog(LOG_INFO,  mosquitto_connack_string(reason_code));
    struct arguments args = *(struct arguments*)obj;
    
	if(reason_code != 0){
		mosquitto_disconnect(mosq);
        return;
	}
    int ret = subscribe_all_topics();
    if(ret <= 0){
        syslog(LOG_INFO, "No topics subscribed, exiting");
        run = 0;
    }
}

/*Sets up user authentication*/
int mqtt_setup_login(struct arguments args)
{
    int ret;
    if(!args.user && !args.password) return MOSQ_ERR_SUCCESS;
    if( (ret = mosquitto_username_pw_set(mosq, args.user, args.password)) != MOSQ_ERR_SUCCESS){
        syslog(LOG_ERR, "User settings error");
        printf("User settings error");
        return ret;
    }
    return MOSQ_ERR_SUCCESS;
}

/*Sets up TLS functionality
Checks if CA file or PSK and IDENTITY are provided*/
int mqtt_setup_tls(struct arguments args)
{
    //both cafile and psk shouldnt be non-null at the same time after parsing args, but just in case, check
    int ret;
    if(args.cafile != NULL){
        if((ret = mosquitto_tls_set(mosq, args.cafile, NULL, args.certfile, args.keyfile, NULL)) != MOSQ_ERR_SUCCESS){
            syslog(LOG_ERR, "TLS set fail");
            return ret;
        }
    } else if(args.psk != NULL){
        if((ret = mosquitto_tls_psk_set(mosq, args.psk, args.identity, NULL)) != MOSQ_ERR_SUCCESS){
            syslog(LOG_ERR, "TLS psk set fail");
            return ret;
        }
    }
    return MOSQ_ERR_SUCCESS;
}

/*Setups mqtt connection
If the CA file is proviced in ARGS, enable TLS connection
If the PSK and identity is provided, enable TLS with pre-shared key
If user information is provided, the method tries to authenticate login
Sets callback functions on connect and message*/
int mqtt_setup(struct arguments args)
{
    int ret;
    if( (ret = mosquitto_lib_init()) != MOSQ_ERR_SUCCESS){
        syslog(LOG_ERR, "Can't initialize mosquitto library");
        return ret;
    }
    mosq = mosquitto_new(NULL, true, &args);
    if( (ret = mqtt_setup_login(args)) != MOSQ_ERR_SUCCESS) return ret;
    if( (ret = mqtt_setup_tls(args)) != MOSQ_ERR_SUCCESS) return ret;

    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    return MOSQ_ERR_SUCCESS;
}

/*signal handler to stop the program loop*/
void sighandler(int signum)
{
    if(signum == SIGTERM || signum == SIGINT){
        run = 0;
    }   
}
/*Tries to reconnect to the broker upon losing connection.
Retry count is defined by COUNT param*/
int mqtt_try_reconnect(struct mosquitto *mosq, int count)
{
    int i = 0;
    int ret;
    while(i < count)
    {
        if(mosquitto_reconnect(mosq) == MOSQ_ERR_SUCCESS)

            return MOSQ_ERR_SUCCESS;
        i++;
        sleep(1);
    }
    run = 0;
    return MOSQ_ERR_CONN_LOST;

}

/*main loop, checks connection and enables message sending*/
int mqtt_loop(struct mosquitto *mosq)
{
    while(run){
        int loop_ret = mosquitto_loop(mosq, -1, 1);
        switch(loop_ret){
            case MOSQ_ERR_CONN_LOST:
                syslog(LOG_ERR, "Lost connection to broker");
                break;
            case MOSQ_ERR_NO_CONN:
                if(mqtt_try_reconnect(mosq, 5) != MOSQ_ERR_SUCCESS)                  
                    syslog(LOG_ERR, "Couldn't reconnect to broker after multiple attemps, exiting");    
                break;
            default:
                break;
        }
    }
}
/*main program functionality*/
int mqtt_main(struct arguments args)
{
    int ret = 0;
    /*register interrupt*/
    struct sigaction act;
    act.sa_handler = sighandler;
    sigaction(SIGINT,  &act, 0);
    sigaction(SIGTERM, &act, 0);
    
    /*start log*/
    openlog("mqtt_sub", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
    setlogmask (LOG_UPTO (LOG_INFO));

    uci_start();
    sqlite_db_start();
    mqtt_setup(args);
    
    ret = mosquitto_connect(mosq, args.host, args.port, 10);
	if(ret != MOSQ_ERR_SUCCESS){
		mosquitto_destroy(mosq);
        uci_end();
        mosquitto_lib_cleanup();
        syslog(LOG_ERR, "Error: %s\n", mosquitto_strerror(ret));
        printf("Error: %s\n", mosquitto_strerror(ret));
		return ret;
	}

    if(args.daemon){
        ret = daemonize();
    }
    if(ret != 0)
    {
        syslog(LOG_ERR, "Failed to create daemon");
        return ret;
    }
    
    mqtt_loop(mosq);

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    uci_end();
    sqlite_db_end();

    syslog(LOG_INFO, "MQTT sub stop");
    return MOSQ_ERR_SUCCESS;
}

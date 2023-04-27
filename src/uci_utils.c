#include <mosquitto.h>
#include "mqtt_utils.h"
#include "uci.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include "events.h"
#include "logger.h"

struct uci_context *ctx;
char subscribed_topics[TOPIC_CAP][MAX_STR_LEN];

/*Loads UCI package*/
int load_package(struct uci_package **package, char *package_path)
{
    char path[MAX_STR_LEN];
    strncpy(path, package_path, sizeof(path));
    int ret;
    if( ret = uci_load(ctx, path, package) != UCI_OK){
        syslog(LOG_ERR, "Cant find package or bad format: %s", package_path);
        return ret;
    }
    return UCI_OK;
}

/*Checks if given option is one of the valid fields, and sets a struct value*/
int set_sender_value_string(struct sender *sender, struct uci_option *o)
{
    if(strcmp(o->e.name, "smtp_port") == 0){
        (*sender).port = atoi(o->v.string);
    } else if(strcmp(o->e.name, "username") == 0){
        strncpy((*sender).user, o->v.string, MAX_STR_LEN);
    } else if(strcmp(o->e.name, "password") == 0){
        strncpy((*sender).password, o->v.string, MAX_STR_LEN);
    } else if(strcmp(o->e.name, "senderemail") == 0){
       strncpy((*sender).email, o->v.string, MAX_STR_LEN);
    } else if(strcmp(o->e.name, "smtp_ip") == 0){
        strncpy((*sender).smtp_ip, o->v.string, MAX_STR_LEN);
    }else if(strcmp(o->e.name, "credentials") == 0){
       (*sender).credentials = atoi(o->v.string);
    } else return FIELD_NOT_FOUND;
    return UCI_OK;
}

/*Gets the sender data from user_groups uci config*/
int get_sender_data(struct sender *sender, char *sender_mail)
{
    struct uci_package *package;
    if(load_package(&package, "user_groups") != UCI_OK)
        return -1;
    int k = 0;
    int found = 0;
    struct uci_element *i;
    uci_foreach_element(&package->sections, i){
        struct uci_section *curr_section = uci_to_section(i);
        struct uci_ptr ptr = {0};

        char path[MAX_STR_LEN];
        sprintf(path, "user_groups.@email[%d].senderemail", k++);

        if(uci_lookup_ptr(ctx, &ptr, path, true) != UCI_OK){
            syslog(LOG_ERR, "No email addressesses are setup in usergroups");
            uci_unload(ctx, package);
            return UCI_ERR_INVAL;
        }
        if(strcmp(ptr.o->v.string, sender_mail) != 0)
            continue;
        
        found = 1;
        struct uci_element *j;
        uci_foreach_element(&curr_section->options, j){
            struct uci_option *o = uci_to_option(j);
            set_sender_value_string(sender, o);
        }
        break;
    }
    uci_unload(ctx, package);

    if(!found){
        syslog(LOG_ERR, "Sender email account not found in usergroups config");
        return SENDER_DATA_ERR;
    }
        
    return UCI_OK;
}

/*Gets a topic's data from the config*/
int get_topic_data(struct uci_section *section, struct topic *topic)
{
    int fields_inserted = 0;
    int field_count = 2;
    struct uci_element *i;
    uci_foreach_element(&section->options, i){
        struct uci_option *o = uci_to_option(i);
        if(strcmp(o->e.name, "name") == 0){
            strncpy((*topic).topic, o->v.string, MAX_STR_LEN);
        } else if(strcmp(o->e.name, "qos") == 0){
            (*topic).qos = atoi(o->v.string);
        } else continue;

        fields_inserted ++;
    }
    if(fields_inserted < field_count){
        syslog(LOG_ERR, "Failed to get topic data, check config");
        return TOPIC_CONFIG_ERR;
    }
    return UCI_OK;
}

/*Checks if given option is one of the valid fields, and sets a struct value*/
int set_event_value_string(struct event *event, struct uci_option *o)
{
    if(strcmp(o->e.name, "topic") == 0){
        strncpy((*event).topic, o->v.string, MAX_STR_LEN);
    } else if(strcmp(o->e.name, "key") == 0){
        strncpy((*event).key, o->v.string, MAX_STR_LEN);
    } else if(strcmp(o->e.name, "comparison") == 0){
        (*event).comparison = atoi(o->v.string);
    } else if(strcmp(o->e.name, "type") == 0){
        (*event).value_type = atoi(o->v.string);
    } else if(strcmp(o->e.name, "value") == 0){
        strncpy((*event).value, o->v.string, MAX_VAL_LEN);
    } else if(strcmp(o->e.name, "sender") == 0){
        strncpy((*event).sender, o->v.string, MAX_STR_LEN);
    } else return FIELD_NOT_FOUND;
    return UCI_OK;
}
/*Checks if given option is one of the valid fields, and sets a struct value*/
int set_event_value_list(struct event *event, struct uci_option *o)
{
    if(strcmp(o->e.name, "receivers") == 0){
        struct uci_element *j;
        int count = 0;
        uci_foreach_element(&o->v.list, j){
            if(count == MAX_RECEIVERS) return UCI_OK;
            strncpy((*event).receivers[count++], j->name, MAX_STR_LEN); 
        }
    } else return FIELD_NOT_FOUND;
    return UCI_OK;
}

/*inserts an event value depending on the uci option type*/
int insert_value_by_type(struct uci_option *o, struct event *event)
{
    int ret;
    switch(o->type){
        case UCI_TYPE_STRING:
            if((ret = set_event_value_string(event, o)) != UCI_OK)
                return ret;
            break;
        case UCI_TYPE_LIST:
            if(ret = (set_event_value_list(event, o)) != UCI_OK)
                return ret;
            break;
    }
    return UCI_OK;
}

/*Checks if an event already exists*/
int check_event_duplicate(struct event event, struct event *events, int count)
{
    for(int i = 0; i < count; i ++){
        if(strcmp(events[i].key, event.key) == 0 
            && strcmp(events[i].topic, event.topic) == 0
            && strcmp(events[i].value, event.value) == 0
            && events[i].value_type == event.value_type
            && events[i].comparison == event.comparison){

            return 1;
        } 
    }
    return 0;
}

/*Gets an event's data from uci config*/
int get_event_data(struct uci_section *section, struct event *event)
{
    int field_count = 7; // variable count in struct event
    int fields_inserted = 0; // counter how many field were actuall in config;
    struct uci_element *i;
     uci_foreach_element(&section->options, i){
        struct uci_option *o = uci_to_option(i);
        int ret = insert_value_by_type(o, event);
        // printf("option %s value %s, return %d\n", o->e.name, o->v.string, ret);
        if(ret != UCI_OK) //TODO: event config validation
            continue;
        fields_inserted ++;
    }
    if(fields_inserted < field_count){
        syslog(LOG_ERR, "Failed to get event data, check config");
        return EVENT_CONFIG_ERR;
    }
    return UCI_OK;
    
}

/*Checks the uci config for any events that match TOPIC and places them into EVENTS*/
int get_topic_events(struct event *events, char *topic)
{
    int count = 0;

    struct uci_package *package;
    if(load_package(&package, "mqtt_sub") != UCI_OK)
        return -1;
    
    struct uci_element *i;
    uci_foreach_element(&package->sections, i){
        if(count == EVENT_CAP){
            syslog(LOG_INFO, "Event cap reached");
            return count;
        } 

        struct uci_section *curr_section = uci_to_section(i);
        if(strcmp(curr_section->type, "event") != 0)
            continue;
            
        struct event event = {0};

        if(get_event_data(curr_section, &event) != UCI_OK){
            continue;
        }
           
        if(strcmp(event.topic, topic) != 0 )
            continue;

        if(check_event_duplicate(event, events, count))
            continue;
        memcpy(&(events[count++]), &event, sizeof(struct event));
        
    }
    uci_unload(ctx, package);
    return count;
}

/*Checks if given topic already exists*/
int check_topic_duplicate(struct topic *topics, struct topic topic, int count){
    for(int i = 0; i < count; i ++){
        if(strcmp(topics[i].topic, topic.topic) == 0 
            && topics[i].qos == topic.qos){

            return 1;
        } 
    }
    return 0;
}

/*Finds all topics in uci config and places them into TOPICS*/
int get_all_topics(struct topic *topics)
{
    int count = 0;
    struct uci_package *package;
    if(load_package(&package, "mqtt_sub") != UCI_OK)
        return -1;
    
    struct uci_element *i;
    uci_foreach_element(&package->sections, i){
        if(count == TOPIC_CAP){
            syslog(LOG_INFO, "Topic cap reached");
            return UCI_OK;
        }

        struct uci_section *curr_section = uci_to_section(i);
        if(strcmp(curr_section->type, "topic") != 0)
            continue;

        struct topic topic = {0};
       
        if(get_topic_data(curr_section, &topic) != UCI_OK)
            continue;
        if(check_topic_duplicate(topics, topic, count))
            continue;
        memcpy(&(topics[count++]), &topic, sizeof(topic));
    }
    uci_unload(ctx, package);
    return count;
}

/*Starts the uci context*/
int uci_start()
{
    ctx = uci_alloc_context();
    if (!ctx) {
        syslog(LOG_ERR, "Failed to allocate UCI context");
        return UCI_ERR_INVAL;
    }
    return UCI_OK;
}
/*Frees the uci context*/
int uci_end()
{
    uci_free_context(ctx);
}
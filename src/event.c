#include <json-c/json.h>
#include <syslog.h>
#include "mqtt_utils.h"
#include "events.h"
#include <string.h>
#include "mail_utils.h"
#include "uci_utils.h"
#include <math.h>

enum{
    CMP_LESS,
    CMP_MORE,
    CMP_EQ,
    CMP_NEQ,
    CMP_LESEQ,
    CMP_MOREQ,
};

/*returns a string for given comparison code*/
char *compare_operator(int comparison)
{
    switch(comparison){
        case CMP_LESS: return "<";
        case CMP_MORE: return ">";
        case CMP_EQ: return "==";
        case CMP_NEQ: return "!=";
        case CMP_MOREQ: return ">=";
        case CMP_LESEQ: return "<=";
    }
}

int compare_numbers(float value1, float value2, int comparison)
{
    switch(comparison){
        case CMP_LESS: return value1 < value2;
        case CMP_MORE: return value1 > value2;
        case CMP_EQ: return value1 == value2;
        case CMP_NEQ: return value1 != value2;
        case CMP_MOREQ: return value1 >= value2;
        case CMP_LESEQ: return value1 <= value2;
    }
}
int compare_strings(char *value1, char *value2, int comparison)
{
    int cmp = strcmp(value1, value2);
    switch(comparison){
        case CMP_EQ: return cmp == 0;
        case CMP_NEQ: return cmp != 0;
    }
}
int comparison_in_contstraint(int comparison, int con1, int con2)
{
    return con1 <= comparison && comparison <= con2;
}

/*Compares two values depending on type.
if type = 0, they are compared as numbers
if type = 1, they are compared as stringd*/
int compare_values(char* value1, char* value2, int type, int comparison)
{
    switch(type){
        case 0:
            if(comparison_in_contstraint(comparison,0,5))
                return compare_numbers(floor(100 * atof(value1)) / 100,
                    floor(100 *atof(value2)) / 100, comparison); //compare number
            break;
        case 1:
            if(comparison_in_contstraint(comparison,2,3))
                return compare_strings(value1, value2,comparison);
            break;
        default:
            break;
    }
    return -1;
}

/*Tries to get a JSON object by given KEY*/
json_object *find_by_key(json_object *obj, char *key)
{     
    struct json_object_iterator current = json_object_iter_begin(obj);
    struct json_object_iterator end = json_object_iter_end(obj);
    
    while (!json_object_iter_equal(&current, &end)) {
        //printf("%s\n", json_object_iter_peek_name(&current));
        if(strcmp(json_object_iter_peek_name(&current), key) == 0){
            return json_object_get(json_object_iter_peek_value(&current));
        }
        json_object_iter_next(&current);
    }
    return NULL; 
}

/*used for displaying data in terminal*/
int print_event(struct event event)
{
    printf("----\n topic: %s\n key: %s\n type: %d\n value: %s\n comparison: %d\n sender:%s\nreceivers:\n",
    event.topic, event.key, event.value_type, event.value, event.comparison, event.sender);
    for(int i = 0; i < MAX_RECEIVERS; i ++)
    {
        if(strcmp(event.receivers[i], "") == 0){
            break;
        } 
        printf(" ......%s\n", event.receivers[i]);
    }
}


/*Does something when event conditions are met*/
int trigger_event(struct event event, char *value_from_payload)
{
    //print_event(event);
    char message[1024];
    snprintf(message, sizeof(message), "Topic: %s\r\nKey: %s\r\nActual value (%s) %s Event value (%s)\r\n",
        event.topic, event.key, value_from_payload, compare_operator(event.comparison), event.value);
    int ret = send_email(event.sender, event.receivers, message);

    return ret;
}

/*Checks if any TOPIC event condition found in PAYLOAD is met */
int check_events(char *topic, char *payload)
{
    int events_triggered = 0;
    struct event events[EVENT_CAP];
    json_object *obj = json_tokener_parse(payload);
    if(!obj){
        syslog(LOG_INFO, "JSON message format is not correct");
        return JSON_ERR;
    }
    json_object *data = find_by_key(obj, "data");

    if(!data){
        syslog(LOG_ERR, "Can't find data object");
        return JSON_ERR;
    }

    char *value_from_payload;
    int count = get_topic_events(events, topic);
    if(count < 0){
        return TOPIC_CONFIG_ERR;
    }
    
    for(int i = 0; i < count; i ++){
        //print_event(events[i]);
        json_object *value = find_by_key(data, events[i].key);
        if(!value){
            //syslog(LOG_ERR,"Can't find value for event key: %s", events[i].key);
            continue;
        }

        value_from_payload = (char*)json_object_get_string(value);
        //printf("%s value from payload: %s\n", events[i].key, value_from_payload);
        int c = compare_values(value_from_payload, events[i].value, events[i].value_type, events[i].comparison);
        if(c > 0){
                //trigger event
                syslog(LOG_INFO, "Event triggered: topic: %s value: %s, comparison: %d, value received: %s",
                    topic, events[i].value, events[i].comparison, value_from_payload);
                trigger_event(events[i], value_from_payload);
                events_triggered++;
        }else if(c < 0) syslog(LOG_ERR, "Unknown comparison detected");
        // printf("%s %s %s, result: %d\n",
        //    events[i].value, compare_operator(events[i].comparison), value_from_payload, c);

    }
    return events_triggered;
}
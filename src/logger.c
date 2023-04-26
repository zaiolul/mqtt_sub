#include "sqlite3.h"
#include <syslog.h>
#include <string.h>
#include <stdio.h>

#define DB_PATH "/log/mqtt_sub.db"
sqlite3 *obj;

/*Opens database file or creates it if not found*/
int sqlite_db_start()
{
    int ret = sqlite3_open_v2(DB_PATH, &obj, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if(ret){
        syslog(LOG_ERR, "Can't open db file");
        return ret;
    }

    char *zErrMsg = 0;
    char *query = 
    "CREATE TABLE IF NOT EXISTS messages("
        "id INTEGER PRIMARY KEY,"
        "timestamp TEXT  NOT NULL,"
        "topic TEXT NOT NULL,"
        "payload TEXT NOT NULL"   
    ")";
    
    ret = sqlite3_exec(obj, query, NULL, 0, &zErrMsg);
    if( ret != SQLITE_OK ){
        syslog(LOG_ERR, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    return 0;
}

/*Closes the database file*/
int sqlite_db_end()
{
    return sqlite3_close_v2(obj);
}

/*Inserts TOPIC and received PAYLOAD to database*/
int sqlite_db_insert_message(char *timestamp, char *topic, char *payload)
{
    if(!obj){
       syslog(LOG_ERR, "Attempt to add to database when it is not opened for editing \n");
        return SQLITE_ERROR;
    }
    int ret;
    
    char *query = 
        "INSERT INTO messages(id, timestamp, topic, payload)"
        "VALUES (NULL, @timestamp, @topic, @payload);";
        
    sqlite3_stmt *res;
    ret = sqlite3_prepare_v2(obj, query, -1, &res, 0);
    if(ret != SQLITE_OK){
        return ret;
    }
    sqlite3_bind_text(res,1,timestamp,strlen(timestamp), NULL);
    sqlite3_bind_text(res,2,topic, strlen(topic),NULL);
    sqlite3_bind_text(res,3,payload, strlen(payload),NULL);
    ret = sqlite3_step(res);
   
    if( ret != SQLITE_DONE ){
        syslog(LOG_ERR, "SQL insert error");
        
        return ret;
    }
    
    sqlite3_finalize(res);
    return SQLITE_OK;
}

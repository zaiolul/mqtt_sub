#ifndef LOGGER_H
#define LOGGER_H

int sqlite_db_start();
int sqlite_db_end();
int sqlite_db_insert_message(char *timestamp, char *topic, char *payload);
#endif
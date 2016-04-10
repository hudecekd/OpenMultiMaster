#ifndef __MYSQL_API_H_
#define __MYSQL_API_H_

//#include "psi_memory.h"
/*
 * my_global.h has to be first otherwise build fails because
 * other header files use macros from my_global.h
 * For example: many errors for PSI can occur.
 */

#include <my_global.h>
#include <mysql.h>
#include <my_sys.h>

#include <syslog.h>

#define DATABASE_NAME "DmrMaster"

#define CONNECTION_TYPE MYSQL *
#define ROW_TYPE MYSQL_ROW

struct RepeaterEntity
{
  int repeaterId;
  char callsign[10];
};

MYSQL *openDatabaseMySql();
void closeDatabaseMySql(MYSQL *connection);
int initDatabaseMySql(MYSQL *connection);

int cleanRegistrations(CONNECTION_TYPE connection, long unixTime);
int cleanTrafficData(CONNECTION_TYPE connection, long timeStamp);
int cleanVoiceTrafficData(CONNECTION_TYPE connection, long timeStamp);

#endif


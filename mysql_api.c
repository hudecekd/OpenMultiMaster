#include "mysql_api.h"
#include "master_server.h"

struct repeater repeaterList[100] = {0};

MYSQL *openDatabaseMySql()
{
  MYSQL *connection = mysql_init(NULL);
  if (connection == NULL)
  {
    syslog(LOG_NOTICE,"Database error: mysql_init");
    return NULL;
  }

  if (mysql_real_connect(connection, "localhost", "root", "servis123", NULL, 0, NULL, 0) == NULL)
  {
    syslog(LOG_NOTICE, "Database error: %s", mysql_error(connection));
    return NULL;
  }
  return connection;
}

void closeDatabaseMySql(MYSQL *connection)
{
  mysql_close(connection);
}

my_bool isFieldExisting(MYSQL *connection, char *db, char *table, char *field)
{
  char buffer[1024];
  char *queryFormat = "SELECT * FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = '%s' AND TABLE_NAME = '%s' AND COLUMN_NAME = '%s'";
  sprintf(buffer, queryFormat, db, table, field);
  if (mysql_query(connection, buffer))
  {
    syslog(LOG_NOTICE,"Database error: %s", mysql_error(connection));
    return 0;
  }

  MYSQL_RES *result = mysql_store_result(connection);
  if (result == NULL)
  {
    syslog(LOG_NOTICE, "Database error: %s", mysql_error(connection));
    return 0;
  }

  my_ulonglong count = mysql_num_rows(result);

  mysql_free_result(result);

  return count == 1;
}

my_bool isTableExisting(MYSQL *connection, char *db, char *table)
{
  char buffer[1024];
  char *queryFormat = "SELECT * FROM information_schema.tables WHERE TABLE_SCHEMA = '%s' AND TABLE_NAME = '%s'";
  sprintf(buffer, queryFormat, db, table);
  if (mysql_query(connection, buffer))
  {
    syslog(LOG_NOTICE,"Database error: %s", mysql_error(connection));
    return 0;
  }

  MYSQL_RES *result = mysql_store_result(connection);
  if (result == NULL)
  {
    syslog(LOG_NOTICE, "Database error: %s", mysql_error(connection));
    return 0;
  }

  my_ulonglong count = mysql_num_rows(result);

  mysql_free_result(result);

  return count == 1;
}

int executeCommand(MYSQL *connection, char *SQLQUERY)
{
  int res = mysql_query(connection, SQLQUERY);
  if (res)
  {
    syslog(LOG_NOTICE, "executeCommand failed with error %d for query %s", res, SQLQUERY);
  }

  my_ulonglong affectedRows = mysql_affected_rows(connection);
}



void initMasterTable(MYSQL *connection)
{
  char SQLQUERY[1000];
  char *format = "\
CREATE TABLE master\
(\
    repTS1 VARCHAR(100) default '',\
    repTS2 VARCHAR(100) default '',\
    sMasterTS1 VARCHAR(100) default '',\
    sMasterTS2 VARCHAR(100) default '',\
    timeBase INTEGER default 60,\
    servicePort int default 50000,\
    rdacPort int default 50002,\
    dmrPort int default 50001,\
    baseDmrPort int default 50100,\
    baseRdacPort int default 50200,\
    maxRepeaters int default 20,\
    echoId int default 9990,\
    rrsGpsId int default 500,\
    aprsUrl VARCHAR(100) default '',\
    aprsPort VARCHAR(7) default '8080',\
    \
    echoSlot integer default 1\
)";

  if (isTableExisting(connection, DATABASE_NAME, "master"))
  {
    syslog(LOG_NOTICE, "master already exists");
    return;
  }

  sprintf(SQLQUERY, format);
  executeCommand(connection, SQLQUERY);
}

void initSMasterTable(MYSQL *connection)
{
  char SQLQUERY[1000];
  char *format = "\
CREATE TABLE sMaster\
(\
    ownName VARCHAR(100) default '',\
    ownCountryCode VARCHAR(5) default '',\
    ownRegion VARCHAR(2) default '',\
    sMasterIp VARCHAR(100) default '',\
    sMasterPort VARCHAR(5) default '62010'\
)";

  if (isTableExisting(connection, DATABASE_NAME, "sMaster"))
  {
    syslog(LOG_NOTICE, "sMaster already exists");
    return;
  }

  sprintf(SQLQUERY, format);
  executeCommand(connection, SQLQUERY);
}

void initRepeatersTable(MYSQL *connection)
{
  char SQLQUERY[1000];
  char *format = "\
CREATE TABLE repeaters\
(\
    repeaterId INTEGER default 0 ,\
    callsign VARCHAR(10) default '',\
    txFreq VARCHAR(10) default '',\
    shift VARCHAR(7) default '',\
    hardware VARCHAR(11) default '',\
    firmware VARCHAR(12) default '',\
    mode VARCHAR(4) default '',\
    currentAddress INTEGER default 0,\
    timeStamp varchar(20) default '1970-1-1 00:00:00',\
    ipAddress VARCHAR(50) default '',\
    language VARCHAR(50) default 'english',\
    geoLocation VARCHAR(20) default '',\
    aprsPass VARCHAR(5) default '0000',\
    aprsBeacon VARCHAR(100) default 'DMR repeater',\
    aprsPHG VARCHHAR(7) default '',\
    \
    currentReflector integer default 0\
    autoReflector integer default 0\
)";

  sprintf(SQLQUERY, format);
  executeCommand(connection, SQLQUERY);
}

void initCallsignsTable(MYSQL *connection)
{
  char SQLQUERY[1000];
  char *format = "\
CREATE TABLE callsigns\
(\
    radioId INTEGER default 0 PRIMARY KEY,\
    callsign VARCHAR(32) default '',\
    name VARCHAR(32) default '',\
    aprsSuffix VARCHAR(3) default '-9',\
    aprsBeacon VARCHAR(100) default 'DMR terminal',\
    aprsSymbol INTEGER default 62,\
    hasSendAprs INTEGER default 0,\
    messageStore INTEGER default 1,\
    email VARCHAR(100) default '',\
    login VARCHAR(50) default '',\
    password VARCHAR(50) default '',\
    lastAprsTime INTEGER default 0,\
    madeChange INTEGER default 0,\
    city VARCHAR(32) default '',\
    state VARCHAR(32) default '',\
    country VARCHAR(32) default '',\
    radio VARCHAR(32) default '',\
    homeRepeaterId VARCHAR(32) default '',\
    remarks VARCHAR(32) default ''\
)";
  sprintf(SQLQUERY, format);
  executeCommand(connection, SQLQUERY);
}

void initRrsTable(MYSQL *connection)
{
  char SQLQUERY[1000];
  char *format = "\
CREATE TABLE rrs\
(\
    radioId INTEGER default 0 PRIMARY KEY,\
    callsign VARCHAR(32) default '',\
    name VARCHAR(32) default '',\
    registerTime VARCHAR(20) default '1970-01-01 00:00:00',\
    onRepeater VARCHAR(32) default '',\
    \
    unixTime long default 0\
)";
  sprintf(SQLQUERY, format);
  executeCommand(connection, SQLQUERY);
}

void initTrafficTable(MYSQL *connection)
{
  char SQLQUERY[1000];
  char *format = "\
CREATE TABLE traffic\
(\
    senderId int default 0 PRIMARY KEY,\
    senderCallsign varchar(32) default '',\
    targetId int default 0,\
    targetCallsign varchar(32) default '',\
    channel int default 0,\
    serviceType varchar(15) default 'Voice',\
    callType varchar(15) default 'Group',\
    timeStamp int default 0,\
    onRepeater varchar(32) default '',\
    senderName varchar(32) default ''\
)";
  sprintf(SQLQUERY, format);
  executeCommand(connection, SQLQUERY);
}
void initVoiceTraffic(MYSQL *connection)
{
  char SQLQUERY[1000];
  char *format = "\
CREATE TABLE voiceTraffic\
(\
    senderId int default 0 PRIMARY KEY,\
    senderCallsign varchar(32) default '',\
    targetId int default 0,\
    targetCallsign varchar(32) default '',\
    channel int default 0,\
    serviceType varchar(15) default 'Voice',\
    callType varchar(15) default 'Group',\
    timeStamp int default 0,\
    onRepeater varchar(32) default '', \
    senderName varchar(32) default ''\
)";
  sprintf(SQLQUERY, format);
  executeCommand(connection, SQLQUERY);
}

void initLocalReflectors(MYSQL *connection)
{
  char SQLQUERY[1000];
  char *format = "\
CREATE TABLE localReflectors\
(\
    id int primary key,\
    name varchar(50)\
)";
  sprintf(SQLQUERY, format);
  executeCommand(connection, SQLQUERY);
}

int initDatabaseMySql(MYSQL *connection)
{
  initCallsignsTable(connection);
  initMasterTable(connection);
  initSMasterTable(connection);
  initRepeatersTable(connection);
  initRrsTable(connection);
  initTrafficTable(connection);
  initVoiceTraffic(connection);
  initLocalReflectors(connection);
}

int updateCallsigns(MYSQL *connection, int radioId)
{
  char SQLQUERY[256];
  sprintf(SQLQUERY,"UPDATE callsigns SET hasSendAprs = 1, lastAprsTime = %lu where radioId = %i",time(NULL),radioId);
  return mysql_query(connection, SQLQUERY);
}

struct CallsignEntity
{
  int radioId;
  char callsign[32];
  char name[32];
  char aprsSuffix[3];
  char aprsBeacon[100];
  int aprsSymbol;
  int hasSendAprs;
  int messageStore;
  char email[100];
  char login[50];
  char password[50];
  int lastAprsTime;
  int madeChange;
  char city[32];
  char state[32];
  char country[32];
  char radio[32];
  char homeRepeaterId[32];
  char remarks[32];
};

struct RepeaterEntity
{
  int repeaterId;
  char callsign[10];
};

int getCallsign(MYSQL *connection, int radioId, struct CallsignEntity *callsign)
{
  int res;
  char SQLQUERY[256];
  sprintf(SQLQUERY,"SELECT callsign,aprsSuffix,aprsBeacon,aprsSymbol,lastAprsTime FROM callsigns WHERE radioId = %i",radioId);
  res = mysql_query(connection, SQLQUERY);
  if (res) { return res; }

  MYSQL_RES *result = mysql_store_result(connection);
  if (result == NULL)
    return 1;

  MYSQL_ROW row = mysql_fetch_row(result);
  sprintf(callsign->callsign, "%s", row[0]);
  sprintf(callsign->aprsSuffix, "%s", row[1]);
  sprintf(callsign->aprsBeacon, "%s", row[2]);
  callsign->aprsSymbol = atoi(row[3]);
  callsign->lastAprsTime = atoi(row[4]);

  mysql_free_result(result);

  return 0;
}

void updateRepeaterStatusInternal(MYSQL *connection, unsigned char currentReflector, char *callsign)
{
  char SQLQUERY[400];
  
  sprintf(SQLQUERY,"UPDATE repeaters set currentReflector = %i where callsign = '%s'",currentReflector, callsign);

  int res = mysql_query(connection, SQLQUERY);
  if (res) { syslog(LOG_NOTICE, "updateRepeaterStatus failed."); }

  my_ulonglong affectedRows = mysql_affected_rows(connection);
}

void updateRepeaterStatus(int status, int reflector, int repPos)
{
  int currentReflector = (status == 2) ? reflector : 0;
  MYSQL *connection = openDatabaseMySql();
  updateRepeaterStatusInternal(connection, reflector, repeaterList[repPos].callsign);
  closeDatabase(connection);
}

int getCallsignTrafficInfo(MYSQL *connection, int radioId, struct CallsignEntity *callsign)
{
  int res;
  char SQLQUERY[256];
  sprintf(SQLQUERY,"SELECT callsign, name FROM callsigns WHERE radioId = %i",radioId);
  res = mysql_query(connection, SQLQUERY);
  if (res) { return res; }

  MYSQL_RES *result = mysql_store_result(connection);
  if (result == NULL)
    return 1;

  MYSQL_ROW row = mysql_fetch_row(result);
  sprintf(callsign->callsign, "%s", row[0]);
  sprintf(callsign->name, "%s", row[1]);

  mysql_free_result(result);

  return 0;
}

int updateTraffic(MYSQL *connection, int senderId, char *senderCallsign, char *senderName, int targetId,
    int channel, char *serviceType, char *callType, /* int timestamp,*/ char *repeater)
{
  char SQLQUERY[256];
  sprintf(SQLQUERY,"REPLACE into traffic (senderId,senderCallsign,senderName,targetId,channel,serviceType,callType,timeStamp,onRepeater) VALUES (%i,'%s','%s',%i,%i,'%s','%s',%lu,'%s')", senderId, senderCallsign, senderName, targetId, channel, serviceType, callType, time(NULL), repeater);

  return executeCommand(connection, SQLQUERY);
}

int updateVoiceTraffic(MYSQL *connection, int senderId, char *senderCallsign, char *senderName, int targetId,
    int channel, char *serviceType, char *callType, /* int timestamp,*/ char *repeater)
{
  char SQLQUERY[256];
  sprintf(SQLQUERY,"REPLACE into voiceTraffic (senderId,senderCallsign,senderName,targetId,channel,serviceType,callType,timeStamp,onRepeater) VALUES (%i,'%s','%s',%i,%i,'%s','%s',%lu,'%s')",senderId, senderCallsign, senderName, targetId, channel, serviceType, callType, time(NULL), repeater);

  return executeCommand(connection, SQLQUERY);
}

void logTraffic(int srcId,int dstId,int slot,unsigned char serviceType[16],char *callType, unsigned char repeater[17])
{
  MYSQL *connection = openDatabaseMySql();

  if (connection == NULL)
    return;

  struct CallsignEntity callsign;
  if (getCallsignTrafficInfo(connection, srcId, &callsign) == 0)
  {
    updateTraffic(connection, srcId, callsign.callsign, callsign.name, dstId, slot, serviceType, callType, /*time(NULL), */ repeater);

    if (strcmp(serviceType, "Voice") == 0)
    {
      // TODO: volam to same znovu proc?
      updateTraffic(connection, srcId, callsign.callsign, callsign.name, dstId, slot, serviceType, callType, /*time(NULL), */ repeater);
    }
  }

  closeDatabase(connection);
}

/*
void updateRepeaterStatus(int status, int reflector, int repPos)
{
  char SQLQUERY[256];
  sprintf(SQLQUERY,"UPDATE repeaters set online = %i, timeStamp = '%s' where callsign = '%s'",status,timeStamp,repeater);

  executeCommand(connection, SQLQUERY);
}
*/

// TODO: rename it because it is used for update, delete, ...

void updateRepeaterTable(MYSQL *connection, int status, int reflector, int repPos)
{
  char SQLQUERY[256];
  int currentReflector = (status == 2) ? reflector : 0;
  sprintf(SQLQUERY,"UPDATE repeaters set currentReflector = %i where callsign = '%s'", currentReflector, repeaterList[repPos].callsign);

  executeCommand(connection, SQLQUERY);
}



/********************* hyteraDecode.c */
int updateRrs(MYSQL *connection, int radioId, char *callsign, char *name, char *registerTime, char *repeaterCallsign, time_t unixTime)
{
  char SQLQUERY[256];
  
  sprintf(SQLQUERY,"REPLACE into rrs (radioId,callsign,name,registerTime,onRepeater,unixTime) VALUES (%i,'%s','%s','%s','%s',%lu)",radioId, callsign,name, registerTime, repeaterCallsign, unixTime);
 
  return executeCommand(connection, SQLQUERY);
}

void decodeHyteraRrs(struct RepeaterEntity *repeater, unsigned char data[300])
{
  int srcId = data[8] << 16 | data[9] << 8 | data[10];
  struct CallsignEntity callsign;
  MYSQL *connection = openDatabaseMySql();

  // getCallsignTrafficInfo contains the same logic we want here
  if (getCallsignTrafficInfo(connection, srcId, &callsign) == 0)
  {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timeStamp[20];
    strftime(timeStamp,sizeof(timeStamp),"%Y-%m-%d %H:%M:%S",t);

    updateRrs(connection,
        srcId, callsign.callsign, callsign.name, timeStamp,
        repeater->callsign, now);
  }

  closeDatabase(connection);
}

void deleteRrs(MYSQL *connection, int radioId)
{
  char SQLQUERY[256];

  sprintf(SQLQUERY,"DELETE FROM rrs where radioId = %i", radioId);
  executeCommand(connection, SQLQUERY);
}

void decodeHyteraOffRrs(struct RepeaterEntity *repeater, unsigned char data[300])
{
  int srcId = data[8] << 16 | data[9] << 8 | data[10];
  struct CallsignEntity callsign;
  MYSQL *connection = openDatabaseMySql();

  // getCallsignTrafficInfo contains the same logic we want here
  if (getCallsignTrafficInfo(connection, srcId, &callsign) == 0)
  {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    deleteRrs(connection, srcId);
  }

  closeDatabase(connection);
}


int getRepeater(MYSQL *connection, struct RepeaterEntity *repeater)
{
  char SQLQUERY[256];
  //sprintf(SQLQUERY,"SELECT repeaterId FROM repeaters WHERE repeaterId = %i",repeaterList[repPos].id);
}

/*
 *  Linux DMR Master server
    Copyright (C) 2014 Wim Hofman

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "master_server.h"

void sendAprsBeacon();
sqlite3 *openDatabase();
void closeDatabase();

void *scheduler(){
	time_t timeNow,beaconTime=0;
	int i;
	bool sending = false;

	syslog(LOG_NOTICE,"Scheduler thread started");
	for(;;){
		sleep(120);
		time(&timeNow);
		//Send APRS beacons
		if (difftime(timeNow,beaconTime) > 1800){
			for(i=0;i<highestRepeater;i++){
				if (repeaterList[i].id != 0 && repeaterList[i].dmrOnline){
					sleep(1);
					sendAprsBeacon(repeaterList[i].callsign,repeaterList[i].aprsPass,repeaterList[i].geoLocation,repeaterList[i].aprsPHG,repeaterList[i].aprsBeacon);
				}
			}
			time(&beaconTime);
			syslog(LOG_NOTICE,"Send aprs beacons for connected repeaters");
		}
		time(&timeNow);
		//Cleanup registration database
                CONNECTION_TYPE connection = openDatabaseMySql();
                cleanRegistrations(connection, time(NULL));
                cleanTrafficData(connection, time(NULL));
                cleanVoiceTrafficData(connection, time(NULL));

                updateRepeaterList(connection);
                closeDatabaseMySql(connection);

		//cleanup dmr not idle
		if (dmrState[1] != IDLE){
			for(i=0;i<highestRepeater;i++){
				if (repeaterList[i].sending[1]){
					sending = true;
					break;
				}
			}
			if (sMaster.sending[1]) sending = true;
			if (!sending){
				dmrState[1] = IDLE;
				syslog(LOG_NOTICE,"DMR state inconsistent on slot 1, settign IDLE");
			}
		}
		sending = false;

		if (dmrState[2] != IDLE){
			for(i=0;i<highestRepeater;i++){
				if (repeaterList[i].sending[2]){
					sending = true;
					break;
				}
			}
			if (sMaster.sending[2]) sending = true;
			if (!sending){
				dmrState[2] = IDLE;
				syslog(LOG_NOTICE,"DMR state inconsistent on slot 2, settign IDLE");
			}
		}

	}
}

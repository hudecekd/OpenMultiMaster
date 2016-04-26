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

#include "mysql_api.h"

#include "master_server.h"

char aprsUrl[100];
char aprsPort[7];
int aprsSockFd;

void openAprsSock(){

	int rv;
	struct addrinfo hints, *servinfo, *p;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	char ipstr[INET_ADDRSTRLEN];

    syslog(LOG_NOTICE,"Opening APRS socket %s %s",aprsUrl,aprsPort);
	if ((rv = getaddrinfo(aprsUrl, aprsPort, &hints, &servinfo)) != 0) {
		syslog(LOG_NOTICE,"getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		struct in_addr *addr;
		struct sockaddr_in *ipv = (struct sockaddr_in *) p->ai_addr;
		addr = &(ipv->sin_addr);
		inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr); 
		syslog(LOG_NOTICE,"address: %s",ipstr);
		if ((aprsSockFd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
			syslog(LOG_NOTICE,"Not able to create APRS sock");
			continue;
		}
		if (connect(aprsSockFd, p->ai_addr, p->ai_addrlen) == -1) {
			close(aprsSockFd);
			syslog(LOG_NOTICE,"Error to connect to APRS");
			continue;
		}
		break;
	}
    if (p == NULL)  syslog(LOG_NOTICE,"Failed to open APRS socket");
	
	freeaddrinfo(servinfo); // all done with this structure	
}

void sendAprsBeacon(char callsign[10],char pass[6],char loc[20],char phg[7],char text[100]){
	char toSend[300];
	char ipstr[INET_ADDRSTRLEN];
	int sockfd;

	sprintf(toSend,"user %s pass %s vers DMRgate 1.0\n%s>APRS,%s,qAR,%s:!%s&%s%s",callsign,pass,callsign,callsign,callsign,loc,phg,text);
        if(!send(aprsSockFd,toSend,strlen(toSend),0)){
                close(aprsSockFd);
                openAprsSock();
                send(aprsSockFd,toSend,strlen(toSend),0);
        }

}

void sendAprs(struct gpsCoordinates gpsData, int radioId,int destId, struct repeater repeater){
	char toSend[300];
	char timeString[7];
	int sockfd;
	struct CallsignEntity radioIdent = {0};
	unsigned char aprsCor[30];

        MYSQL *connection = openDatabaseMySql();
        getCallsign(connection, radioId, &radioIdent);
	if (time(NULL) - radioIdent.lastAprsTime < 5){
		syslog(LOG_NOTICE,"[%s]Preventing aprs.fi flooding for %s",repeater.callsign,radioIdent.callsign);
                closeDatabaseMySql(connection);
		return;
	}

        updateCallsignsAprsTime(connection, radioId);
        closeDatabaseMySql(connection);
	sprintf(aprsCor,"%s/%s>%s/%s",gpsData.latitude,gpsData.longitude,gpsData.heading,gpsData.speed);
	aprsCor[18] = radioIdent.aprsSymbol;

	//overwrite symbol if destId != 500
	switch(destId){

        case 5050:	//fixed (home)
        aprsCor[18] = 45;
		sprintf(radioIdent.aprsSuffix,"");
        break;
		
		case 5055:	//Different network  -5 (node)
		aprsCor[18] = 110;
		sprintf(radioIdent.aprsSuffix,"-5");
		break;

		case 5056:	//Special activity -6  (Tent)
		aprsCor[18] = 59;
		sprintf(radioIdent.aprsSuffix,"-6");
		break;
	
		case 5057:	//Handheld -7 (Runner)
		aprsCor[18] = 91;
		sprintf(radioIdent.aprsSuffix,"-7");
		break;
	
		case 5058:	//mobile station (Camping car,motor cycle,Bicicle) -8  (RV)
		aprsCor[18] = 82;
		sprintf(radioIdent.aprsSuffix,"-8");
		break;

		case 5059:	//mobile station  -9 (car)
		aprsCor[18] = 62;
		sprintf(radioIdent.aprsSuffix,"-9");
		break;
	}
	sprintf(toSend,"user %s pass %s vers DMRgate 1.0\n%s%s>APRS,%s,qAR,%s:!%s %s\n",repeater.callsign,repeater.aprsPass,radioIdent.callsign,radioIdent.aprsSuffix,repeater.callsign,repeater.callsign,aprsCor,radioIdent.aprsBeacon);

	if(!send(aprsSockFd,toSend,strlen(toSend),0)){
		close(aprsSockFd);
		openAprsSock();
		send(aprsSockFd,toSend,strlen(toSend),0);
	}
	syslog(LOG_NOTICE,"[%s]Send info to APRS network for %s [%s]",repeater.callsign,radioIdent.callsign,toSend);
}


int checkCoordinates(struct gpsCoordinates gpsData, struct repeater repeater){

        regex_t regex;
        int reti;



        reti = regcomp(&regex, "^[0-9][0-9][0-9][0-9][.][0-9][0-9][NZ]$", 0);
        if(reti){
                syslog(LOG_NOTICE,"[%s]Could not compile regex latitude",repeater.callsign);
				regfree(&regex);
                return 0;
        }
        reti = regexec(&regex,gpsData.latitude,0,NULL,0);
        if(reti == REG_NOMATCH){
                syslog(LOG_NOTICE,"[%s]Corrupt latitude received",repeater.callsign);
                regfree(&regex);
                return 0;
        }
		regfree(&regex);
		
        reti = regcomp(&regex, "^[0-9][0-9][0-9][0-9][0-9][.][0-9][0-9][EW]$", 0);
        if(reti){
                syslog(LOG_NOTICE,"[%s]Could not compile regex longitude",repeater.callsign);
                regfree(&regex);
                return 0;
        }

        reti = regexec(&regex,gpsData.longitude,0,NULL,0);
        if(reti == REG_NOMATCH){
                syslog(LOG_NOTICE,"[%s]Corrupt longitude received",repeater.callsign);
                regfree(&regex);
                return 0;
        }
		regfree(&regex);
		
        reti = regcomp(&regex, "^[0-9][0-9][0-9]$", 0);
        if(reti){
                syslog(LOG_NOTICE,"[%s]Could not compile regex heading",repeater.callsign);
                regfree(&regex);
                return 0;
        }

        reti = regexec(&regex,gpsData.heading,0,NULL,0);
        if(reti == REG_NOMATCH){
                syslog(LOG_NOTICE,"[%s]Corrupt heading received",repeater.callsign);
                regfree(&regex);
                return 0;
        }
		regfree(&regex);

        reti = regcomp(&regex, "^[0-9.][0-9.][0-9.]$", 0);
        if(reti){
                syslog(LOG_NOTICE,"[%s]Could not compile regex speed",repeater.callsign);
                regfree(&regex);
                return 0;
        }

        reti = regexec(&regex,gpsData.speed,0,NULL,0);
        if(reti == REG_NOMATCH){
                syslog(LOG_NOTICE,"[%s]Corrupt speed received",repeater.callsign);
                regfree(&regex);
                return 0;
        }

        regfree(&regex);
	return 1;
} 

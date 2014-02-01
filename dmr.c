#include "master_server.h"

#define NUMSLOTS 2                                        //DMR IS 2 SLOT
#define SLOT1 4369                                        //HEX 1111
#define SLOT2 8738                                        //HEX 2222
#define VCALL 4369                                        //HEX 1111
#define DCALL 26214                                        //HEX 6666
#define ISSYNC 61166										//HEX EEEE
#define VCALLEND 8738										//HEX 2222
//#define CALL 2
//#define CALLEND 3
#define PTYPE_ACTIVE1 2                                        
#define PTYPE_END1 3
#define PTYPE_ACTIVE2 66
#define PTYPE_END2 67
#define VFRAMESIZE 72                                        //UDP PAYLOAD SIZE OF REPEATER VOICE/DATA TRAFFIC
#define SYNC_OFFSET1 18                                        //UDP OFFSETS FOR VARIOUS BYTES IN THE DATA STREAM
#define SYNC_OFFSET2 19                                        //
//#define SYNC_OFFSET3 18                                        //Look for EEEE
//#define SYNC_OFFSET4 19                                        //Look for EEEE
#define SLOT_OFFSET1 16                                        //        
#define SLOT_OFFSET2 17
#define PTYPE_OFFSET 8
#define SRC_OFFSET1 68
#define SRC_OFFSET2 69
#define SRC_OFFSET3 70
#define DST_OFFSET1 64
#define DST_OFFSET2 65
#define DST_OFFSET3 66
#define TYP_OFFSET1 62

struct allow{
	bool repeater;
	bool sMaster;
	bool isRange;
};
void delRdacRepeater();

struct allow checkTalkGroup(int dstId, int slot, int callType){
	struct allow toSend = {0};
	int i;
	
	toSend.isRange = false;
	//First check sMaster talk groups. All sMaster talkgroups apply to the repeaters also
	if (slot == 1){
		for(i=0;i<=master.sMasterTS1GroupCount;i++){
			if (dstId >= sMasterTS1List[i][0] && dstId <= sMasterTS1List[i][1]){
				toSend.repeater = true;
				toSend.sMaster = true;
				if (sMasterTS1List[i][0] != sMasterTS1List[i][1]) toSend.isRange = true;
				return toSend;
			}
		}
	}
	else{
		for(i=0;i<=master.sMasterTS2GroupCount;i++){
			if (dstId >= sMasterTS2List[i][0] && dstId <= sMasterTS2List[i][1]){
				toSend.repeater = true;
				toSend.sMaster = true;
				if (sMasterTS2List[i][0] != sMasterTS2List[i][1]) toSend.isRange = true;
				return toSend;
			}
		}
	}
	
	if (slot == 1){
		for(i=0;i<=master.repTS1GroupCount;i++){
			if (dstId >= repTS1List[i][0] && dstId <= repTS1List[i][1]){
				toSend.repeater = true;
				toSend.sMaster = false;
				if (repTS1List[i][0] != repTS1List[i][1]) toSend.isRange = true;
				return toSend;
			}
		}
	}
	else{
		for(i=0;i<=master.repTS2GroupCount;i++){
			if (dstId >= repTS2List[i][0] && dstId <= repTS2List[i][1]){
				toSend.repeater = true;
				toSend.sMaster = false;
				if (repTS2List[i][0] != repTS2List[i][1]) toSend.isRange = true;
				return toSend;
			}
		}
	}
	toSend.repeater = false;
	toSend.sMaster = false;
	syslog(LOG_NOTICE,"Talk group %i not allowed on slot",dstId,slot);
	return toSend;
}

void *dmrListener(void *f){
	int sockfd,n,i,rc;
	struct sockaddr_in servaddr,cliaddr;
	socklen_t len;
	unsigned char buffer[VFRAMESIZE];
	unsigned char response[VFRAMESIZE] ={0};
	int repPos = (intptr_t)f;
	int packetType = 0;
	int sync = 0;
	int srcId = 0;
	int dstId = 0;
	int callType = 0;
	unsigned char slot = 0;
	unsigned char voiceSeq[3],prevVoiceSeq[3];
	long voicePacketsReceived[3];
	long voicePacketsLost[3];
	fd_set fdMaster;
	struct timeval timeout;
	time_t timeNow,pingTime;
	struct allow toSend = {0};
	bool block[3];
	unsigned char sMasterFrame[103];
	char myId[11];
	unsigned char webUserInfo[100];
	//{0x00,0x00,0x00,0x00,0x32,0x30,0x34,0x31,0x39,0x2e,0x37}

	syslog(LOG_NOTICE,"DMR thread for port %i started",baseDmrPort + repPos);
	sockfd=socket(AF_INET,SOCK_DGRAM,0);
	repeaterList[repPos].sockfd = sockfd;
	repeaterList[repPos].dmrOnline = true;
	repeaterList[repPos].sending[1] = false;
	repeaterList[repPos].sending[2] = false;
	block[1] = false;
	block[2] = false;
	voicePacketsReceived[1] = 0;
	voicePacketsReceived[2] = 0;
	voicePacketsLost[1] = 0;
	voicePacketsLost[2] = 0;
	//create frame to append after packet for sMaster
	memset(sMasterFrame,0,103);
	memcpy(myId,(char*)&repeaterList[repPos].id,sizeof(int));
	memcpy(myId+4,master.ownCountryCode,4);
	memcpy(myId+7,master.ownRegion,1);
	memcpy(myId+8,version,3);
    
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(baseDmrPort + repPos);
	bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	FD_ZERO(&fdMaster);
	time(&pingTime);
	len = sizeof(cliaddr);
	
	for (;;){
		FD_SET(sockfd, &fdMaster);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		if (rc = select(sockfd+1, &fdMaster, NULL, NULL, &timeout) == -1) { 
			syslog(LOG_NOTICE,"Select error, closing socket port %i",baseDmrPort + repPos);
			close(sockfd);
			pthread_exit(NULL);
        }
		
		if (FD_ISSET(sockfd,&fdMaster)) {
			n = recvfrom(sockfd,buffer,VFRAMESIZE,0,(struct sockaddr *)&cliaddr,&len);
		
			if (n>2){
				slot = buffer[SLOT_OFFSET1] / 16;
				if (dmrState[slot] == IDLE || repeaterList[repPos].sending[slot]){
					packetType = buffer[PTYPE_OFFSET];
					sync = buffer[SYNC_OFFSET1] << 8 | buffer[SYNC_OFFSET2];
					if (sync == VCALL){
						voicePacketsReceived[slot]++;
						voiceSeq[slot] = buffer[4];
						if (prevVoiceSeq[slot] !=0 && prevVoiceSeq[slot] + 1 != voiceSeq[slot]) voicePacketsLost[slot]++;
						if (voiceSeq[slot] != 0xff) prevVoiceSeq[slot] = voiceSeq[slot]; else prevVoiceSeq[slot] = 0;
					}
					switch (packetType){
				
						case 0x01:
						if (sync == VCALL && dmrState[slot] != VOICE && block[slot] == false){
							sMasterFrame[98] = slot;
							srcId = buffer[SRC_OFFSET3] << 16 | buffer[SRC_OFFSET2] << 8 | buffer[SRC_OFFSET1];
							dstId = buffer[DST_OFFSET3] << 16 | buffer[DST_OFFSET2] << 8 | buffer[DST_OFFSET1];
							callType = buffer[TYP_OFFSET1];
							repeaterList[repPos].sending[slot] = true;
							sprintf(webUserInfo,"RX_Slot=%i,GROUP=%i,USER_ID=%i,TYPE=Voice,VERS=%s,RPTR=%i,%s\n",slot,dstId,srcId,version,repeaterList[repPos].id,master.ownName);
							if (sMaster.online){
								sendto(sMaster.sockfd,webUserInfo,strlen(webUserInfo),0,(struct sockaddr *)&sMaster.address,sizeof(sMaster.address));
							}
							toSend = checkTalkGroup(dstId,slot,callType);
							if (toSend.repeater == false){
								block[slot] = true;
								break;
							}
							if(toSend.isRange && dstId != master.ownCCInt){
								memcpy(sMasterFrame+90,(char*)&master.ownCCInt,sizeof(int));
							}
							else{
								memset(sMasterFrame+90,0,4);
							}
							dmrState[slot] = VOICE;
							syslog(LOG_NOTICE,"[%i-%s]Voice call started on slot %i src %i dst %i type %i",baseDmrPort + repPos,repeaterList[repPos].callsign,slot,srcId,dstId,callType);
							break;
						}
						if (sync == DCALL && dmrState[slot] != DATA){
							srcId = buffer[SRC_OFFSET3] << 16 | buffer[SRC_OFFSET2] << 8 | buffer[SRC_OFFSET1];
							dstId = buffer[DST_OFFSET3] << 16 | buffer[DST_OFFSET2] << 8 | buffer[DST_OFFSET1];
							callType = buffer[TYP_OFFSET1];
							toSend.sMaster = false;
							syslog(LOG_NOTICE,"[%i-%s]Data on slot %i src %i dst %i type %i",baseDmrPort + repPos,repeaterList[repPos].callsign,slot,srcId,dstId,callType);
							break;
						}
						break;
				
						case 0x03:
						if (sync == VCALLEND){
							dmrState[slot] = IDLE;
							repeaterList[repPos].sending[slot] = false;
							if (voicePacketsReceived[slot] == 0) voicePacketsReceived[slot] = 1;
							syslog(LOG_NOTICE,"[%i-%s]Voice call ended on slot %i packet loss %.2f %%",baseDmrPort + repPos,repeaterList[repPos].callsign,slot,(double)((voicePacketsLost[slot]/voicePacketsReceived[slot])*100));
							if (block[slot] == true) syslog(LOG_NOTICE,"[%i-%s] But was blocked because of not allowed talk group",baseDmrPort + repPos,repeaterList[repPos].callsign);
							block[slot] = false;
							prevVoiceSeq[slot] = 0;
							voicePacketsReceived[slot] = 0;
							voicePacketsLost[slot] = 0;
						}
						break;
					}
					if (!block[slot]){
						for (i=0;i<highestRepeater;i++){
							if (repeaterList[i].address.sin_addr.s_addr !=0 && !repeaterList[i].sending[slot]){
								sendto(repeaterList[i].sockfd,buffer,n,0,(struct sockaddr *)&repeaterList[i].address,sizeof(repeaterList[i].address));
							}
						}
						if (toSend.sMaster && sMaster.online){
							memcpy(sMasterFrame,buffer,n);
							memcpy(sMasterFrame + n,myId,11);
							sendto(sMaster.sockfd,sMasterFrame,103,0,(struct sockaddr *)&sMaster.address,sizeof(sMaster.address));
						}
					}
				}
			}
			else{
				response[0] = 0x41;
				sendto(sockfd,response,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
				time(&pingTime);
			}
		}
		else{
			time(&timeNow);
			if (repeaterList[repPos].sending[1] && dmrState[1] != IDLE){
				if (voicePacketsReceived[1] == 0) voicePacketsReceived[1] = 1;
				if (dmrState[1] = VOICE) syslog(LOG_NOTICE,"[%i-%s]Voice call ended after timeout on slot 1 packet loss %.2f %%",baseDmrPort + repPos,repeaterList[repPos].callsign,(double)((voicePacketsLost[1]/voicePacketsReceived[1])*100));
				dmrState[1] = IDLE;
				repeaterList[repPos].sending[1] = false;
				block[1] = false;
				syslog(LOG_NOTICE,"[%i-%s]Slot 1 IDLE",baseDmrPort + repPos,repeaterList[repPos].callsign);
				prevVoiceSeq[1] = 0;
				voicePacketsReceived[1] = 0;
				voicePacketsLost[1] = 0;
			}
			if (repeaterList[repPos].sending[2] && dmrState[2] != IDLE){
				if (voicePacketsReceived[2] == 0) voicePacketsReceived[2] = 1;
				if (dmrState[2] = VOICE) syslog(LOG_NOTICE,"[%i-%s]Voice call ended after timeout on slot 2 packet loss %.2f %%",baseDmrPort + repPos,repeaterList[repPos].callsign,(double)((voicePacketsLost[2]/voicePacketsReceived[2])*100));
				dmrState[2] = IDLE;
				repeaterList[repPos].sending[2] = false;
				block[2] = false;
				syslog(LOG_NOTICE,"[%i-%s]Slot 2 IDLE",baseDmrPort + repPos,repeaterList[repPos].callsign);
				prevVoiceSeq[2] = 0;
				voicePacketsReceived[2] = 0;
				voicePacketsLost[2] = 0;
			}
			if (difftime(timeNow,pingTime) > 60 && !repeaterList[repPos].sending[slot]){
				syslog(LOG_NOTICE,"PING timeout on DMR port %i repeater %s, exiting thread",baseDmrPort + repPos,repeaterList[repPos].callsign);
				syslog(LOG_NOTICE,"Removing repeater from list position %i",repPos);
				repeaterList[repPos].address.sin_addr.s_addr = 0;
				repeaterList[repPos].sockfd = 0;
				repeaterList[repPos].dmrOnline = false;
				if (repPos + 1 == highestRepeater) highestRepeater--;
				delRdacRepeater(cliaddr);
				close(sockfd);
				pthread_exit(NULL);
			}
		}
	}
}

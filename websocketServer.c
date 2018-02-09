#define DEBUG					0
#define WEBSOCKET_DECODE		1   /* Websocket decode option */
#define WEBSOCKET_SAVE			1	/* Save to file option */
#define HANDSHAKE_PRINT			0	/* Save to file option */

#define IMAGE_NAME				"/conf/SDCARD/jerry_tmp/250"	/* BMC TX file path  */
#define RECEICE_FILE	   		"/conf/SDCARD/jerry_tmp/receiveFromClientFile" /* BMC RX file path  */

#define DEFEULT_SERVER_PORT		8888
#define MAX_RX_LEN				1*1024*1024
#define MAX_TX_LEN				1*1024*1024
#define MAX_WEB_SOCKET_KEY_LEN	256
#define modeTX 					"TX"
#define modeRX					"RX"
#define RECEIVEBUFFERSIZE		1024*1024*125

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include "encoder.h"
#include <time.h>
#include <stdbool.h>
#include <pthread.h>

/* Create thread */ 
pthread_t	Websocket_receive;
pthread_t	Websocket_decode;

unsigned char rxBuf[MAX_RX_LEN];
unsigned char txBuf[MAX_TX_LEN];

void usage(char *name) {
	printf("Usage: %s [Port Number(Default:%d)] [TX/RX]\n", name, DEFEULT_SERVER_PORT);
}

int recvFromClient(int clientFd) {
	unsigned char finFlag, maskFlag, mask[4];
	unsigned int payLoadDataLength[8];
	unsigned long payloadLen;
	int i;
	int headLen;
	int remainSize;
	int read_size = 0;
	int write_file_size,maskCount = 0;
	int alreadySize;
	int firstSectionDataLength;
    double cpu_time_used;
	float rate,sizeData;
	clock_t start, end;
#ifdef WEBSOCKET_SAVE
		FILE *received_file;
#endif

	start = clock();	/* receive start time */

	memset(rxBuf, 0, MAX_RX_LEN);

	if(WEBSOCKET_SAVE)
	{
		received_file = fopen(RECEICE_FILE, "w");
		printf("File open!!\n\n");
	}else{
		printf("No WEBSOCKET_SAVE option\n\n");
	}

	/* Receive first section data */
	read_size = recv(clientFd, rxBuf, RECEIVEBUFFERSIZE, 0);
	if(read_size <= 0)
	{
		printf("\nREAD ERROR!!\n\n");
		close(clientFd);
		exit(0);
	}
	if(DEBUG)
		printf("read_size: %d\n",read_size);

	if(DEBUG)
	{
		for(i = 0; i < read_size; i++)
		{
			if (i > 0 && (i%8) != 0) printf(":");
			if ((i%8)==0) printf("  ");
			if ((i%16)==0) printf("\n");
			printf("%02X", rxBuf[i]);
		}
	}
	
	/* parser Header */
	printf("Start parser Header...\n\n");

	finFlag=((rxBuf[0] & 0x80) == 0x80);
	payloadLen=(rxBuf[1] & 0x7F);
	if(DEBUG)
		printf("payloadLen: %ld\n",payloadLen);
	maskFlag=((rxBuf[1] & 0x80) == 0x80);

	/* Get mask and payLoad Data Length */
	switch(payloadLen)
	{
		case 126:
			memcpy(mask, rxBuf+4, 4);
			*payLoadDataLength = ((*(rxBuf+2) & 0xff ) << 8 )| (*(rxBuf+3) & 0xff );
			headLen = 4;
			break;
		case 127:
			memcpy(mask, rxBuf+10, 4);
			*payLoadDataLength = ((*(rxBuf+6) & 0xff ) << 24 )|((*(rxBuf+7) & 0xff ) << 16 )|((*(rxBuf+8) & 0xff ) << 8 )|(*(rxBuf+9) & 0xff );
			headLen = 10;
			break;
		default:
			memcpy(mask, rxBuf+2, 4);
			*payLoadDataLength = payloadLen;
			headLen = 2;
			break;
	}
	if(DEBUG)
		printf("payLoadDataLength: %d\n",*payLoadDataLength);
	
	firstSectionDataLength = read_size - headLen - 4;  /* first data payload length (total - header - mask length) */

	for( i=0 ; i < firstSectionDataLength ; i++) {
		/* websocket decode	*/
		rxBuf[i] = (unsigned char)(rxBuf[i + headLen + 4] ^ mask[maskCount % 4]);
		maskCount++;
	}

	if(WEBSOCKET_SAVE)
		write_file_size = fwrite(rxBuf, sizeof(char), firstSectionDataLength, received_file);

	alreadySize = firstSectionDataLength; 

	/* parser Header finished */
	printf("Parser Header finished!!\n\n");

	if(WEBSOCKET_DECODE)
	{
		/* Continoue receive payload data, decode and write to the file  */
		printf("Start receive and decode payload data...\n\n");
	}else{
		printf("Start receive payload data(without decode)...\n\n");	
	}

	remainSize = *payLoadDataLength - firstSectionDataLength;

	while(remainSize > 0)
	{
		if( remainSize < RECEIVEBUFFERSIZE )
		{
			read_size = recv(clientFd, rxBuf, remainSize, 0);
		}else{
			read_size = recv(clientFd, rxBuf, RECEIVEBUFFERSIZE, 0);
		}

		if(WEBSOCKET_DECODE)
		{
			for( i = 0 ; i < read_size ; i++) {
				/* websocket decode	*/
				rxBuf[i] = (unsigned char)(rxBuf[i] ^ mask[maskCount % 4]);
				maskCount++;
			}

			if(DEBUG)
			{
				if(maskCount<2){
					for (i = 0; i < 4; i++){printf("[%d]",mask[i]);}
					printf("\n");
					printf("\n\n---(%d)after---\n",maskCount);
					for(i = 0; i < read_size; i++)
					{
						if (i > 0 && (i%8) != 0) printf(":");
						if ((i%8)==0) printf("  ");
						if ((i%16)==0) printf("\n");
						printf("%02X", rxBuf[i]);
					}
				}			
			}
		}

		if(WEBSOCKET_SAVE)
			write_file_size = fwrite(rxBuf, sizeof(char), read_size, received_file);

		remainSize -= read_size;
		alreadySize += read_size;

		if(DEBUG)
			printf("\nwrite_file_size: %d\n\n",write_file_size);

		if(DEBUG)
			printf("maskCount: %d, read_size: %d, remainSize: %d, alreadySize: %d\n", maskCount, read_size, remainSize, alreadySize);

	}
	printf("Receive and decode payload data finished!!\n\n");

	if(WEBSOCKET_SAVE)
	{
		fclose(received_file);
		printf("File close!!\n\n");
	}

	end = clock();  /* receive end time */

	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	rate = (*payLoadDataLength / 1024.00 / 1024.00 / cpu_time_used);
	sizeData = *payLoadDataLength / 1024.00/ 1024.00;
	printf("\nPerformance of BMC RX:\n");
	printf("-------------------------- \n");
	printf("Size: %5.2f MB\n", sizeData);
	printf("Time: %5.2f Sec\n", cpu_time_used);
	printf("Rate: %5.2f MB/s \n", rate);
	printf("-------------------------- \n");

	return 0;
}

int fromWebSocketHeader(unsigned char *sendBuffer, unsigned long picSize, int *headLen){
	*sendBuffer=130;
	if( picSize < 126 ) {
		*(sendBuffer+1)=picSize;
		*headLen=2;
	} else if (picSize < 65537 ) { // 2 byte
		*(sendBuffer+1)=126;
		*(sendBuffer+2)= ((picSize & 0xff00) >> 8);
		*(sendBuffer+3)= (picSize & 0xff);
		*headLen=4;
	} else {
		*(sendBuffer+1)=127;
		*(sendBuffer+2)=0;
		*(sendBuffer+3)=0;
		*(sendBuffer+4)=0;
		*(sendBuffer+5)=0;
		*(sendBuffer+6)= ((picSize & 0xff000000) >> 24);
		*(sendBuffer+7)= ((picSize & 0xff0000) >> 16);
		*(sendBuffer+8)= ((picSize & 0xff00) >> 8);
		*(sendBuffer+9)= (picSize & 0xff);
		*headLen=10;
	}
	return 0;
}

int sendToClient(int clientFd) {
	int imgFd;
	struct stat imgSt;
	unsigned int readSize;
	unsigned long remainSize;
	int headLen=0;
	unsigned char txBuf[MAX_TX_LEN];
	struct timeval completeTime;
	struct timeval completeTimeStart;
	float sec,usec;
	double mb,total;
	
	gettimeofday(&completeTimeStart , NULL);

	stat( IMAGE_NAME, &imgSt);
	remainSize=imgSt.st_size;
	mb = imgSt.st_size;

	imgFd = open( IMAGE_NAME, O_RDONLY);
	if(imgFd < 0) {
	   printf("Error Opening Image File\n");
	   return -1;
	} 
	
	memset(txBuf, 0, MAX_TX_LEN);
	headLen=0;

	fromWebSocketHeader(txBuf, remainSize, &headLen);

	if( remainSize > (MAX_TX_LEN-headLen) )
		readSize = read(imgFd, txBuf + headLen, MAX_TX_LEN-headLen);
	else
		readSize = read(imgFd, txBuf + headLen, remainSize);
	remainSize -= readSize;
	readSize += headLen;
    write(clientFd, txBuf, readSize);                        
	
	//printf("[remainSize:%d readSize:%d]\n",remainSize,readSize);
	printf("\nSending file... size:%5.2lfMB\n\n",mb/1024.00/1024.00);
	while(remainSize) {
		memset(txBuf, 0, MAX_TX_LEN);
		if ( remainSize >= MAX_TX_LEN) {
			readSize = read(imgFd, txBuf, MAX_TX_LEN);
		} else {
			readSize = read(imgFd, txBuf, remainSize);
		}
    	write(clientFd, txBuf, readSize);                        
		remainSize -= readSize;
		//printf("[remainSize:%d]\n",remainSize);
	}

	gettimeofday(&completeTime , NULL);
	printf("---------------------------\n");
	printf("Size: %5.2lf MB\ntime: %5.2f s\nRate: %5.2lf MB/s \n", mb/1024.00/1024.00, ((completeTime.tv_sec-completeTimeStart.tv_sec)+((completeTime.tv_usec - completeTimeStart.tv_usec)*0.000001)),
		(mb/1024/1024)/((completeTime.tv_sec-completeTimeStart.tv_sec)+((completeTime.tv_usec - completeTimeStart.tv_usec)*0.000001)));
	printf("---------------------------\n");

	if ( imgFd )
		close(imgFd);

	return 0;
}

char * calculateSHA1(char * data) {
	char * sha1DataTemp;
	char * sha1Data;
	int i, n=0;
	sha1DataTemp=sha1_hash(data);
	n=strlen(sha1DataTemp); 
	sha1Data=(char *)malloc(n/2+1);
	memset(sha1Data,0,n/2+1);
	
	for(i=0;i<n;i+=2)
	{
	  sha1Data[i/2]=htoi(sha1DataTemp,i,2);
	}
	return sha1Data;
}

char * fetchSecKey()  
{  
	static char key[MAX_WEB_SOCKET_KEY_LEN];
	char *keyBegin;
	char *flag="Sec-WebSocket-Key: ";
	int i=0, bufLen=0;
	memset(key, 0, MAX_WEB_SOCKET_KEY_LEN);
	keyBegin=strstr(rxBuf,flag);
	if(!keyBegin){
		return NULL;
	}
	keyBegin+=strlen(flag);

	bufLen=strlen(rxBuf);
	for(i=0;i<bufLen;i++){
		if( (keyBegin[i]==0x0A) || (keyBegin[i]==0x0D) ) {
			break;
		}
		key[i]=keyBegin[i];
	}
	return key;
}

char * calculateKey() {
	char * clientKey;
	const char * GUID="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	char * sha1Result;
	clientKey=fetchSecKey();
	if(!clientKey) {
		return NULL;
	}
	strcat(clientKey,GUID);
	sha1Result=calculateSHA1(clientKey);
	return base64_encode(sha1Result, strlen(sha1Result));
}

int handshake(int clientFd)
{
	if(!clientFd)
		return -1;

	memset(txBuf, 0, MAX_TX_LEN);
	sprintf(txBuf, "HTTP/1.1 101 Switching Protocols\r\n");  
	sprintf(txBuf, "%sUpgrade: websocket\r\n", txBuf);  
	sprintf(txBuf, "%sConnection: Upgrade\r\n", txBuf);  
	sprintf(txBuf, "%sSec-WebSocket-Accept: %s\r\n\r\n", txBuf, calculateKey());  
	 
	if(HANDSHAKE_PRINT)
	{
		printf("Response Header:%s\n\n",txBuf);
	}
	write(clientFd,txBuf,strlen(txBuf));  
	return 0;
}

int main(int argc, char *argv[]) {
	int port=DEFEULT_SERVER_PORT;
	int serverFd, clientFd;
	struct sockaddr_in servAddr, cliAddr;
	socklen_t cliAddrLen;
	int flag=1, readCnt;
	char cwd[1024];
	char mode;
	char imageName;

	if( argc > 1 ) {
		port = atoi(argv[1]);
	}

	if(port <=0 || port > 0xFFFF) {
		printf("Port(%d) is out of range(1-%d)\n",port,0xFFFF);  
		usage(argv[0]);
		return -1;  
	}

	serverFd = socket(AF_INET, SOCK_STREAM, 0);  
	
	bzero(&servAddr, sizeof(servAddr));  
	servAddr.sin_family = AF_INET;  
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);  
	servAddr.sin_port = htons(port);  
	if( setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1 ) {
		printf("set socket options error.\n");
		return -1;
	}

	bind(serverFd, (struct sockaddr *)&servAddr, sizeof(servAddr));  
	listen(serverFd, 1);
	cliAddrLen=sizeof(cliAddr);
	printf("\nWaiting for connection at Port(%d)...\n\n", port);
	clientFd=accept(serverFd, (struct sockaddr *)&cliAddr, &cliAddrLen);
	printf("From %s at PORT %d\n\n",  inet_ntoa(cliAddr.sin_addr),  ntohs(cliAddr.sin_port));
	memset(rxBuf, 0, MAX_RX_LEN);

	readCnt=read(clientFd, rxBuf, MAX_RX_LEN);
	if(HANDSHAKE_PRINT)
	{
		printf("--------- Read Data From Client(%d) --------------\n", readCnt);
		printf("%s", rxBuf);
		printf("----------------------------------------------\n\n");
	}

	handshake(clientFd);

	if( strcmp(argv[2], modeRX))
	{
		sendToClient(clientFd);
	}
	else if( strcmp(argv[2], modeTX) )
	{
		recvFromClient(clientFd);
	}else{
		usage(argv[0]);
	}

	return 0;
}


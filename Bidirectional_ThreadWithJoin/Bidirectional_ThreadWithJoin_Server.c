#define DEBUG					0
#define WEBSOCKET_DECODE		1   /* Websocket decode option */
#define WEBSOCKET_SAVE			0	/* Save to file option */
#define HANDSHAKE_PRINT			0	/* Save to file option */

#define IMAGE_NAME				"/conf/SDCARD/jerry_tmp/250"	/* BMC TX file path  */
#define RECEICE_FILE	   		"/conf/SDCARD/jerry_tmp/receiveFromClientFile" /* BMC RX file path  */

#define DEFEULT_SERVER_PORT		8888
#define MAX_RX_LEN				2*1024*1024
#define MAX_TX_LEN				2*1024*1024
#define MAX_WEB_SOCKET_KEY_LEN	256
#define modeTX 					"TX"
#define modeRX					"RX"
#define RECEIVEBUFFERSIZE		1024*1024*2
#define ACK_KEY                 "WebsocketOK"
#define DECODE_BUF_LEN			1024*1024*2 

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
unsigned char mask[4];
unsigned int maskCount = 0;
unsigned char deBuf[DECODE_BUF_LEN];

FILE *received_file;

void *webSocketDecode(void *param);
int	pthread_attr_setstacksize(pthread_attr_t *tattr, size_t size);
pthread_mutex_t mutex;

void usage(char *name) {
	printf("Usage: %s [Port Number(Default:%d)] [TX/RX]\n", name, DEFEULT_SERVER_PORT);
}

int recvFromClient(int clientFd) {
	unsigned char finFlag, opcode, maskFlag;
	unsigned int payLoadDataLength[8];
	unsigned long payloadLen;
	int i;
	int headLen;
	int remainSize;
	int read_size = 0;
	int alreadySize;
	int firstSectionDataLength = 0;
	pthread_t decode_th, write_th;
	int decode_ret, write_ret;
	int *decode_arg = malloc(sizeof(*decode_arg));
	//int *write_arg = malloc(sizeof(*write_arg));
	//pthread_attr_t attr;
	//size_t size;



	memset(rxBuf, 0, MAX_RX_LEN);
	memset(mask, 0, 4);

	/* Receive first section data */
	read_size = recv(clientFd, rxBuf, RECEIVEBUFFERSIZE, 0);
	if(read_size <= 0)
	{
		printf("\nREAD ERROR!! read size < 0\n\n");
		close(clientFd);
		exit(0);
	}

	finFlag = rxBuf[0] & 0x80 >> 7;
	opcode = rxBuf[0] & 0x0f; 
	if(opcode == 8)
	{
		printf("Websocket receive finished!!! opcode: %d\n\n",opcode);
		return -1;	
	}
	payloadLen = (rxBuf[1] & 0x7F);
	maskFlag = ((rxBuf[1] & 0x80) == 0x80);
	if(DEBUG)
	{
		printf("finFlag: %d, opcode: %d\n", finFlag, opcode);
		printf("payloadLen: %ld\n",payloadLen);
	}

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
	
	firstSectionDataLength = read_size - headLen - 4;  /* first data payload length (total - header - mask length) */

	if(WEBSOCKET_DECODE)
	{
		if(firstSectionDataLength > 0)
		{
			//size = 1024 * 1024 * 5;
			//pthread_attr_init (&attr);
			//pthread_attr_setstacksize(&attr, size);
			*decode_arg = firstSectionDataLength;
			printf ("A read_size = %d\n",firstSectionDataLength);
			decode_ret = pthread_create(&decode_th, NULL, (void*)webSocketDecode, (void*)decode_arg);
			if(decode_ret != 0)
			{	
				printf ("decode_ret = %d\n",decode_ret);
				printf ("Create decode A pthread error!\n");
				exit (1);
			}
			pthread_join(decode_th, NULL);
		}
	}

	if(WEBSOCKET_SAVE)
	{
		if(firstSectionDataLength > 0)
		{
			webSocketWrite(firstSectionDataLength);
		}
	}

	alreadySize = firstSectionDataLength; 
	remainSize = *payLoadDataLength - firstSectionDataLength;

	if(DEBUG)
	{
		printf("payLoadDataLength: %d\n", *payLoadDataLength);
		printf("alreadySize: %d\n", alreadySize);
		printf("remainSize: %d\n", remainSize);
	}

	while(remainSize > 0)
	{
		/* receive remain data */
		if( remainSize < RECEIVEBUFFERSIZE )
		{
			read_size = recv(clientFd, rxBuf, remainSize, 0);
		}else{
			read_size = recv(clientFd, rxBuf, RECEIVEBUFFERSIZE, 0);
		}

		if(WEBSOCKET_DECODE)
		{
			//size = 1024 * 1024 * 5;
			//pthread_attr_init (&attr);
			//pthread_attr_setstacksize(&attr, size);
			*decode_arg = read_size;
			printf ("B read_size = %d\n",read_size);
			decode_ret = pthread_create(&decode_th, NULL, (void*)webSocketDecode, (void*)decode_arg);
			if(decode_ret != 0)
			{	
				printf ("decode_ret = %d\n",decode_ret);
				printf ("Create decode B pthread error!\n");
				exit (1);
			}
			pthread_join(decode_th, NULL);
		}

		if(WEBSOCKET_SAVE)
		{
			webSocketWrite(read_size);
		}

		remainSize -= read_size;
		alreadySize += read_size;
	}
	free(decode_arg);
	//free(write_arg);
	return alreadySize;
}

void *webSocketDecode(void *param){
	int i;
	int decodeLength = *((int *) param);

	printf("decodeLength = %d\n",decodeLength);
	//pthread_mutex_lock(&mutex);
	for( i = 0 ; i < decodeLength ; i++) {
		rxBuf[i] ^= mask[maskCount % 4];
		maskCount++;
	}
	//pthread_mutex_unlock(&mutex);
	pthread_exit(0);
}
/*
int webSocketDecode(int decodeLength) {
	int i;

	for( i = 0 ; i < decodeLength ; i++) {
		rxBuf[i] ^= mask[maskCount % 4];
		maskCount++;
	}

	return 0;
}
*/
int webSocketWrite(long dataLength) {
	long write_file_size = 0;

	write_file_size = fwrite(rxBuf, sizeof(char), dataLength, received_file);

	return 0;
}

int ackToClient(int clientFd) {
	unsigned long remainSize, readSize;
	int headLen = 0, ret, i;
	unsigned char* ackBuffer;
	
	ackBuffer = (char *)malloc(10);
	memset(ackBuffer, 0, 10);
	headLen=0;
	remainSize = strlen(ACK_KEY);

	fromWebSocketHeader(ackBuffer, remainSize, &headLen);

	strncpy(ackBuffer+2, ACK_KEY, remainSize);

	if(DEBUG)
	{
	/*
		for(i = 0;i < remainSize + headLen ;i++)
		{
			printf("ackBuffer: %X\n",ackBuffer[i]);
		}
	*/
	}

  	ret = write(clientFd, ackBuffer, remainSize + headLen);

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
	printf("Size: %5.2f MB\n", mb/1024.00/1024.00);
	printf("Time: %5.2f Sec\n", ((completeTime.tv_sec-completeTimeStart.tv_sec)+((completeTime.tv_usec - completeTimeStart.tv_usec)*0.000001)));
	printf("Rate: %5.2f MB/s \n", (mb/1024/1024)/((completeTime.tv_sec-completeTimeStart.tv_sec)+((completeTime.tv_usec - completeTimeStart.tv_usec)*0.000001)));
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

int receiveClient(int clientFd) {
	int status = 0;
	long totalSize = 0;

	status = recvFromClient(clientFd);
	totalSize += status;
	while(!ackToClient(clientFd))
	{
		status = recvFromClient(clientFd);
		totalSize += status;
		if(status == -1)
		{
			break;
		}
	}
	return totalSize;
}

int main(int argc, char *argv[]) {
	int port=DEFEULT_SERVER_PORT;
	int serverFd, clientFd;
	struct sockaddr_in servAddr, cliAddr;
	socklen_t cliAddrLen;
	int flag=1, readCnt;
	char mode;
	char imageName;
	clock_t start, end;
    double cpu_time_used;
	long fileTotalSize;
	double mb, rate;

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
		if(WEBSOCKET_SAVE)
			received_file = fopen(RECEICE_FILE, "w");
		start = clock();	/* receive start time */
		fileTotalSize = receiveClient(clientFd);
		end = clock();  /* receive end time */
		if(WEBSOCKET_SAVE)
			fclose(received_file);
		//cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
		cpu_time_used = ((double) (end - start)) / 1000000;
		mb = fileTotalSize / 1024.00 / 1024.00;
		rate = mb / cpu_time_used;
		printf("Size: %5.2f MB\n", mb);
		printf("Time: %5.2f Sec\n", cpu_time_used);
		printf("Rate: %5.2f MB/s \n\n", rate);
	}else{
		usage(argv[0]);
	}

	return 0;
}


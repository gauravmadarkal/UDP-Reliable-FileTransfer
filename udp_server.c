/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/time.h>
#include <dirent.h>
#include <time.h>

#define BUFSIZE 4096
#define PACKET_SIZE 10240
enum msgTypes { NEW, ACK };
enum commands { GET, PUT, DELETE, LIST, MSG };

enum MSG_ATTRIBUTES { COMMAND, SEQ_NO, PACKET_COUNT, FILE_NAME, MSG_TYPE };
struct Packet {
	int command;
	int seq_number;
	char filename[100];
	int msgType;
	char packetContents[PACKET_SIZE];
    char message[100];
	int relatedPacketsCount;
	int fileSize;
};

/*
 * error - wrapper for perror
 */
void error(char *msg) {
	perror(msg);
	exit(1);
}

char* getPacketContents(char *file, int increment, char packetContents[PACKET_SIZE]) {
	// char packetContents[PACKET_SIZE];
	// packetContents = malloc(PACKET_SIZE * sizeof(char));
	int startIndex = (increment * PACKET_SIZE);
	int endIndex = 0;
	int j = 0;
	if (startIndex == 0) 
		endIndex = PACKET_SIZE;
	else 
		endIndex = startIndex * 2;
	for (int i = startIndex; i < endIndex; i+=1) {
		packetContents[j] = file[i];
		j+=1;
	}
	return packetContents;
}

void getTimeStamp(char tvalue[256]) {
	time_t seconds;
    seconds = time(NULL);
    sprintf(tvalue, "%ld", seconds);
}

void getFileExtension(char filename[100], char extension[10]) {
	int dotIndex = 0, i, j = 0;
	for (i = 0; i < strlen(filename); i++) {
		if (filename[i] == '.') {
			dotIndex = i;
		}
	}
	printf("%d", dotIndex);
	for (i = dotIndex; i < strlen(filename); i++) {
		extension[j] = filename[i];
		j++;
	}
	extension[j] = '\0';
}

int main(int argc, char **argv) {

	int sockfd; /* socket */
	int portno; /* port to listen on */
	int clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent *hostp; /* client host info */
	char buf[BUFSIZE]; /* message buf */
	char msg[PACKET_SIZE];
	char packetContents[PACKET_SIZE];
	// packetContents = malloc (PACKET_SIZE * sizeof(char));
	char *hostaddrp; /* dotted decimal host addr string */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */
	char command[7];
	char filename[100], extension[10];
	char *newFilename;
	newFilename = malloc (100 * sizeof(char));
	char timestamp[256];
	int i, j=0;
	FILE *fp;
	char *file = NULL;
	file = malloc (1048576 * sizeof *file);
	char ch;
	int fileSize = 0;
	int sendPacketCount = 0, currentPacketCount = 1, relatedPacketsCount;
	struct Packet packet;
	int sequenceNumber;
	struct timeval tv;
	DIR *d;
    struct dirent *dir;
	char *fileList;
	fileList = malloc (PACKET_SIZE * sizeof(char));
	char files[PACKET_SIZE];


	/* 
	 * check command line arguments 
	 */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	portno = atoi(argv[1]);

	/* 
	 * socket: create the parent socket 
	 */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	tv.tv_sec = 10;
	tv.tv_usec = 0;
	/* setsockopt: Handy debugging trick that lets 
	 * us rerun the server immediately after we kill it; 
	 * otherwise we have to wait about 20 secs. 
	 * Eliminates "ERROR on binding: Address already in use" error. 
	 */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, 
			(const char*)&tv, sizeof tv);

	// setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	// 		 (const void *)&optval , sizeof(int));

	/*
	 * build the server's Internet address
	 */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)portno);

	/* 
	 * bind: associate the parent socket with a port 
	 */
	if (bind(sockfd, (struct sockaddr *) &serveraddr, 
		 sizeof(serveraddr)) < 0)
		error("ERROR on binding");
	
	/* 
	 * main loop: wait for a datagram, then echo it
	 */
	clientlen = sizeof(clientaddr);
	while (1) {
		/*
		 * recvfrom: receive a UDP datagram from a client
		*/
		bzero(msg, PACKET_SIZE);
		n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
			(struct sockaddr *) &clientaddr, &clientlen);
		if (n > 0) {
			sequenceNumber = packet.seq_number;
			if (packet.command == GET) {
				printf("Get file %s \n", packet.filename);
				fp = fopen(packet.filename, "r");
				if (fp == NULL) {
					printf("Invalid file \n");
					packet.seq_number = sequenceNumber;
					packet.command = MSG;
					packet.msgType = NEW;
					strcpy(packet.message, "File not found");
					n = sendto(sockfd, &packet, sizeof(packet), 0, 
						(struct sockaddr *) &clientaddr, clientlen);
				}
				else {
					do {
						ch = fgetc(fp);
						file[i] = ch;
						i+=1;fileSize+=1;
					} while (ch != EOF);
					fileSize = fileSize * sizeof(char);
					sendPacketCount = 1;
					if (fileSize > PACKET_SIZE) {
						sendPacketCount = (int)(fileSize / PACKET_SIZE);
						if ((PACKET_SIZE * sendPacketCount) < fileSize) {
							sendPacketCount += 1;
						}
					}
					i=0;
					strcpy(packet.packetContents, getPacketContents(file, i, packetContents));
					sequenceNumber += 1;
					packet.seq_number = sequenceNumber;
					packet.command = GET;
					packet.msgType = NEW;
					packet.relatedPacketsCount = sendPacketCount;
					packet.fileSize = fileSize;
					printf("----------------------------------------------------- \n");
					printf("Sending first packet - seqNo: %d, total packet count: %d \n", packet.seq_number, packet.relatedPacketsCount);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, 
						(struct sockaddr *) &clientaddr, clientlen);
					if (sendPacketCount == 1) {
						recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
								(struct sockaddr *) &clientaddr, &clientlen);
						if (packet.msgType == ACK) {
							printf("Transfer complete");
						}
					} else {
						while(currentPacketCount != sendPacketCount) {
							// rc = poll(fds, 1, 3000);
							n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
								(struct sockaddr *) &clientaddr, &clientlen);
							if (n < 0) {
								printf("----------------------------------------------------- \n");
								printf("Packet not received after timeout, sending seqNo: %d again \n", packet.seq_number);
								n = sendto(sockfd, &packet, sizeof(packet), 0, 
									(struct sockaddr *) &clientaddr, clientlen);
							}
							else if (packet.msgType == ACK) {
								printf("----------------------------------------------------- \n");
								printf("Received ACK for Packet seqNo: %d \n", packet.seq_number);
								currentPacketCount += 1; sequenceNumber += 1, i += 1;
								strcpy(packet.packetContents, getPacketContents(file, i, packetContents));
								packet.seq_number = sequenceNumber;
								packet.command = GET;
								packet.msgType = NEW;
								printf("----------------------------------------------------- \n");
								printf("Sending next packet seqNo: %d \n", packet.seq_number);
								n = sendto(sockfd, &packet, sizeof(packet), 0, 
									(struct sockaddr *) &clientaddr, clientlen);
							} else {
								printf("Unknown packet received: %d \n", packet.msgType);
							}
						}
					}
				}
			} 
			else if (packet.command == PUT) {
				relatedPacketsCount = packet.relatedPacketsCount;
				fileSize = packet.fileSize;
				i = 0, currentPacketCount = 1;
				strcpy(filename, packet.filename);
				memset(file, 0, sizeof file);
				printf("----------------------------------------------------- \n");
				printf("Received Packet seqNo: %d \n\n", packet.seq_number);
				memcpy(file + (i * PACKET_SIZE), packet.packetContents, PACKET_SIZE * sizeof(char));
				packet.command = PUT;
				packet.msgType = ACK;
				bzero(packet.packetContents, PACKET_SIZE);
				// strcpy(packet.packetContents, NULL);
				printf("Sending ACK for seqNo: %d \n", packet.seq_number);
				n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&clientaddr, clientlen);
				i += 1; currentPacketCount += 1;
				while(currentPacketCount <= relatedPacketsCount) {
					n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&clientaddr, &clientlen);
					packet.seq_number = sequenceNumber;
					packet.command = PUT;
					packet.msgType = ACK;
					bzero(packet.packetContents, PACKET_SIZE);
					// strcpy(packet.packetContents, NULL);
					printf("----------------------------------------------------- \n");
					printf("Received Packet seqNo: %d \n\n", packet.seq_number);
					memcpy(file + (i * PACKET_SIZE), packet.packetContents, PACKET_SIZE * sizeof(char));
					printf("Sending ACK for seqNo: %d \n", packet.seq_number);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&clientaddr, clientlen);
					i += 1; sequenceNumber += 1; currentPacketCount += 1;
				}
				// create the file
				printf("Full file received \n\n");
				// printf("%s \n\n", file);
				getTimeStamp(timestamp);
				getFileExtension(filename, extension);
				strcat(newFilename, timestamp);
				strcat(newFilename, extension);
				printf("New file created: %s \n", newFilename);
				fp = fopen(newFilename, "w");
				if (fp == NULL) {
					printf("Invalid file");
					return 0;
				}
				for (i = 0; i < (fileSize / sizeof(char)); i++) {
					fprintf(fp, "%c", file[i]);
				}
				fclose(fp);

			} 
			else if (packet.command == DELETE) {
				n = remove(packet.filename);
				sequenceNumber += 1;
				packet.seq_number += 1;
				packet.msgType = NEW;
				packet.command = DELETE;
				if (n < 0) {
					strcpy(packet.message, "Could not delete file");
				} else {
					strcpy(packet.message, "File deleted successfully");
				}
				n = sendto(sockfd, &packet, sizeof(struct Packet), 0, 
					(struct sockaddr *) &clientaddr, clientlen);
				n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
					(struct sockaddr *) &clientaddr, &clientlen);
				if (packet.msgType == ACK) {
					printf("Delete Request completed \n");
				}
			} 
			else if (packet.command == LIST) {
				// list files
				d = opendir(".");
				if (d)
				{
					while ((dir = readdir(d)) != NULL)
					{
						strcat(fileList, dir->d_name);
						strcat(fileList, "\n");
					}
					for (i = 0; i < strlen(fileList); i++) {
						files[i] = fileList[i];
					}
					closedir(d);
					sequenceNumber += 1;
					packet.command = LIST;
					packet.msgType = NEW;
					strcpy(packet.packetContents, files);
					packet.seq_number = sequenceNumber;
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, 
						(struct sockaddr *) &clientaddr, clientlen);
					n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
							(struct sockaddr *) &clientaddr, &clientlen);
					if (n < 0) {
						printf("Receive Timeout \n");
					}
					if (packet.msgType == ACK) {
						printf("Listing Request completed \n");
					}
				}
			} 
			else {
				printf("Invalid command received");
			}
		} else {
			printf("Waiting message from client \n");
		}
	}
}

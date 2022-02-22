/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <time.h>

#define BUFSIZE 4096
#define PACKET_SIZE 10240
enum msgTypes { NEW, ACK };
enum commands { GET, PUT, DELETE, LIST, MSG };
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
	exit(0);
}

char* getPacketContents(char *file, int increment, char packetContents[PACKET_SIZE]) {
	// char packetContents[PACKET_SIZE];
	// packetContents = malloc(PACKET_SIZE * sizeof(char));
	int startIndex = (increment * PACKET_SIZE);
	int endIndex = 0;
	int j = 0, i = 0;
	if (startIndex == 0) 
		endIndex = PACKET_SIZE;
	else 
		endIndex = startIndex + PACKET_SIZE;
	for (i = startIndex; i < endIndex; i+=1) {
		packetContents[j] = file[i];
		j+=1;
	}
	return packetContents;
}

void getTimeStamp(char* tvalue) {
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
	for (i = dotIndex; i < strlen(filename); i++) {
		extension[j] = filename[i];
		j++;
	}
	extension[j] = '\0';
}


int main(int argc, char **argv) {
	int sockfd, portno, n;
	int serverlen;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname;
	char buf[BUFSIZE];
	// char *buf = (char*)malloc(BUFSIZE);
	char msg[PACKET_SIZE];
	char *file = NULL;  /* declare a pointer, and initialize to NULL */
	// max file receivable is 500MB
	file = malloc (504857600);
	char serverResponse[BUFSIZE];
	FILE *fp;
	char ch;
	int i, currentPacketCount = 1;
	char *fname;
	char filename[100], extension[10];
	char *newFilename;
	newFilename = malloc(500);
	char *timestamp;
	timestamp = malloc(500);
	char *command;
    struct Packet packet;
	int sequenceNumber = 0;
	char *packetContents;
	packetContents = malloc (PACKET_SIZE * sizeof(char));
	int relatedPacketsCount = 0;
	int fileSize = 0;
	int commandValidity = 1, sendPacketCount = 0;

	/* check command line arguments */
	if (argc != 3) {
	   fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
	   exit(0);
	}
	hostname = argv[1];
	portno = atoi(argv[2]);

	/* socket: create the socket */ 
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	/* gethostbyname: get the server's DNS entry */
	server = gethostbyname(hostname);
	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host as %s\n", hostname);
		exit(0);
	}

	/* build the server's Internet address */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
	serveraddr.sin_port = htons(portno);
	serverlen = sizeof(serveraddr);

	/* get a message from the user */
	// printf("Please enter msg: ");
	while(1) {
		bzero(buf, BUFSIZE);
		// take input command from user
		printf("Type a command \n 1. Get <File name>, 2. Put <File name>, 3. Delete <File name>, 4. List, 5. Exit \n");
		fgets(buf, BUFSIZE, stdin);
		command = strtok(buf, " ");
		fname = strtok(NULL, " ");
		sequenceNumber += 1;
		packet.seq_number = sequenceNumber;
		packet.msgType = NEW;

		// read the file name from the input
		if (fname != NULL) {
			for (i = 0; i < strlen(fname); i++) {
				if (!(fname[i] == '\n')) {
					filename[i] = fname[i];
				}
			}
		}
		filename[i] = '\0';
		if (strcmp(command, "Put") == 0) {
			// read the contents of the file into a buffer
			fp = fopen(filename, "r");
			if (fp == NULL) {
				printf("Invalid file, give full path to file. eg: Put ./client/test.txt \n");
				continue;
			}
			i=0;
			fseek(fp, 0, SEEK_END);
			long fsize = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			char *file = malloc(fsize + 1);
			fread(file, fsize, 1, fp);
			fclose(fp);
			file[fsize] = 0;
			packet.command = (int)PUT;
			// calculate the number of packets to be sent
			sendPacketCount = 1;
			if (fsize > PACKET_SIZE) {
				sendPacketCount = (int)(fsize / PACKET_SIZE);
				if ((PACKET_SIZE * sendPacketCount) < fsize) {
					sendPacketCount += 1;
				}
			}
			i=0;
			// copy first packet contents
			memcpy(packet.packetContents, getPacketContents(file, i, packetContents), PACKET_SIZE);
			sequenceNumber += 1;
			packet.seq_number = sequenceNumber;
			packet.command = PUT;
			packet.msgType = NEW;
			packet.relatedPacketsCount = sendPacketCount;
			packet.fileSize = fsize;
			strncpy(packet.filename, filename, strlen(filename));
			printf("----------------------------------------------------- \n");
			printf("Sending first packet - seqNo: %d, total packet count: %d \n", packet.seq_number, packet.relatedPacketsCount);
			n = sendto(sockfd, &packet, sizeof(struct Packet), 0, 
				(struct sockaddr *) &serveraddr, serverlen);
			if (sendPacketCount == 1) {
				// if only 1 packet to be sent, then wait for the ACK
				recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
						(struct sockaddr *) &serveraddr, &serverlen);
				if (packet.msgType == ACK) {
					printf("Transfer complete");
					continue;
				}
			} else {
				// if more than 1 packet to be sent, then follow send and wait protocol
				while(currentPacketCount != sendPacketCount) {
					// receive the most recent packet's ACK
					n = recvfrom(sockfd, &packet, sizeof(struct Packet), 0,
						(struct sockaddr *) &serveraddr, &serverlen);
					if (n < 0) {
						// if ACK receive timed out then resend the previous packet 
						printf("----------------------------------------------------- \n");
						printf("Packet not received after timeout, sending seqNo: %d again \n", packet.seq_number);
						n = sendto(sockfd, &packet, sizeof(packet), 0, 
							(struct sockaddr *) &serveraddr, serverlen);
					}
					// if ACK of that packet is received then send next paket
					else if (packet.msgType == ACK) {
						printf("----------------------------------------------------- \n");
						printf("Received ACK for Packet seqNo: %d \n", packet.seq_number);
						currentPacketCount += 1; sequenceNumber += 1, i += 1;
						// get packet contents in increments of PACKET_SIZE
						memcpy(packet.packetContents, getPacketContents(file, i, packetContents), PACKET_SIZE);
						packet.seq_number = sequenceNumber;
						packet.command = PUT;
						packet.msgType = NEW;
						printf("----------------------------------------------------- \n");
						printf("Sending next packet seqNo: %d \n", packet.seq_number);
						n = sendto(sockfd, &packet, sizeof(packet), 0, 
							(struct sockaddr *) &serveraddr, serverlen);
					} else {
						printf("Unknown packet received: %d \n", packet.msgType);
					}
				}
			}
		}
		else if (strcmp(command, "Get") == 0) {
			packet.command = (int)GET;
			strcpy(packet.filename, filename);
		}
		else if (strcmp(command, "Delete") == 0) {
			packet.command = (int)DELETE;
			strcpy(packet.filename, filename);
		}
		else if (strcmp(command, "List\n") == 0) {
			packet.command = (int)LIST;
		}
		else if (strcmp(buf, "Exit\n") == 0) {
			printf("Goodbye!!!, exiting...\n");
			return 0;
		} 
		else {
			commandValidity = 0;
			printf("Command  not recognized, please enter valid command \n");
		}
		if (commandValidity == 1) {
			// send the command to server
			n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
			if (n < 0)
				printf("error in send \n");
			// receive the response
			n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serveraddr, &serverlen);
			printf("Received command: %d \n", packet.command);
			if (n < 0) 
				error("Error in receive \n");
			if (packet.msgType == NEW) {
				sequenceNumber += 1;
				// if received response for GET request, prepare to receive a FILE
				if (packet.command == GET) {
					// number of continous packets of same type
					relatedPacketsCount = packet.relatedPacketsCount;
					fileSize = packet.fileSize;
					i = 0, currentPacketCount = 1;
					memset(file, 0, sizeof file);
					printf("----------------------------------------------------- \n");
					printf("Received Packet seqNo: %d \n\n", packet.seq_number);
					// COPY contents from first packet
					memcpy(file + (i * PACKET_SIZE), packet.packetContents, PACKET_SIZE);
					packet.command = GET;
					packet.msgType = ACK;
					bzero(packet.packetContents, PACKET_SIZE);
					printf("Sending ACK for seqNo: %d \n", packet.seq_number);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
					i += 1; currentPacketCount += 1;
					// recursively keep receiving following packets if many more
					while(currentPacketCount <= relatedPacketsCount) {
						n = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)&serveraddr, &serverlen);
						if (packet.seq_number == sequenceNumber + 1) {
							packet.seq_number = sequenceNumber;
							packet.command = GET;
							packet.msgType = ACK;
							memcpy(file + (i * PACKET_SIZE), packet.packetContents, PACKET_SIZE);
							bzero(packet.packetContents, PACKET_SIZE);
							printf("----------------------------------------------------- \n");
							printf("Received Packet seqNo: %d \n\n", packet.seq_number);
							// keep copying the packet contents to a file buffer
							printf("Sending ACK for seqNo: %d \n", packet.seq_number);
							n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
							i += 1; sequenceNumber += 1; currentPacketCount += 1;
						}
						else {
							printf("Received an out of order packet, dropping it \n");
						}
					}
					// create the file
					printf("Full file received \n\n");
					// create the file with the received contents
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
				else if (packet.command == LIST) {
					printf("--------------------Files on server--------------------\n");
					printf("%s \n", packet.packetContents);
					printf("--------------------END--------------------\n");
					packet.msgType = ACK;
					bzero(packet.packetContents, PACKET_SIZE);
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
				}
				else if (packet.command == DELETE) {
					printf("%s \n", packet.message);
					packet.msgType = ACK;
					n = sendto(sockfd, &packet, sizeof(struct Packet), 0, (struct sockaddr *)&serveraddr, serverlen);
				}
				else if (packet.command == MSG) {
					printf("Received message from server: %s \n", packet.message);
				}
			}
		}
	}
}

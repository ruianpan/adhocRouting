/*
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#include "gpioLib.h"

#define DEST_MAC0	0xdc
#define DEST_MAC1	0xa6
#define DEST_MAC2	0x32
#define DEST_MAC3	0xf7
#define DEST_MAC4	0xde 
#define DEST_MAC5	0x19

//other device I believe
#define MY_DEST_MAC0	0xdc
#define MY_DEST_MAC1	0xa6
#define MY_DEST_MAC2	0x32
#define MY_DEST_MAC3	0xf7
#define MY_DEST_MAC4	0xe7
#define MY_DEST_MAC5	0x36

#define ETHER_TYPE	0x0800

#define DEFAULT_IF	"wlan0"
#define BUF_SIZ		1024
#define FRAME_HDR_SIZE  14
#define GPIO_PIN	17
#define GPIO_PIN_DONE   27

//number of lines of csv data to parse
#define NUM_ITERS	700

int initRX(char *ifName){
	int sockfd, sockopt;
	if( (sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0){
		perror("listening socket");	
		exit(1);
	}

	/* Set interface to promiscuous mode - do we need to do this every time? */
	struct ifreq ifopts;	/* set promiscuous mode */
	strncpy(ifopts.ifr_name, ifName, IFNAMSIZ-1);
	ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(sockfd, SIOCSIFFLAGS, &ifopts);

	/* Allow the socket to be reused - incase connection is closed prematurely */
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) < 0){
		perror("setsockopt");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	/* Bind to device */
	if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifName, IFNAMSIZ-1) < 0){
		perror("SO_BINDTODEVICE");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	return sockfd;
}


int initTX(int *ifindex, uint8_t *src_mac, char *ifName){
	int sockfd;

	/* Open RAW socket to send on */
	if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) < 0) {
	    perror("socket");
	    exit(1);
	}

	struct ifreq tmp;
	memset(&tmp, 0, sizeof(struct ifreq));
	strncpy(tmp.ifr_name, ifName, IFNAMSIZ-1);

	if (ioctl(sockfd, SIOCGIFINDEX, &tmp) < 0) perror("SIOCGIFINDEX");
	*ifindex= tmp.ifr_ifindex;

	if (ioctl(sockfd, SIOCGIFHWADDR, &tmp) < 0) perror("SIOCGIFHWADDR");
	memcpy((void*)src_mac, (void*)(tmp.ifr_hwaddr.sa_data), ETH_ALEN);

	return sockfd;
}

int RX(int RXsockfd, struct ether_header *eh, char *buf, int *prev_sec, int *prev_usec){
	int numbytes;
	numbytes = recvfrom(RXsockfd, buf, BUF_SIZ, 0, NULL, NULL);
	printf("Frame of %lu data bytes recv'd\n", numbytes - FRAME_HDR_SIZE);

	if (eh->ether_dhost[0] == DEST_MAC0 &&
			eh->ether_dhost[1] == DEST_MAC1 &&
			eh->ether_dhost[2] == DEST_MAC2 &&
			eh->ether_dhost[3] == DEST_MAC3 &&
			eh->ether_dhost[4] == DEST_MAC4 &&
			eh->ether_dhost[5] == DEST_MAC5) {
		
		struct timeval tv;
		tv.tv_sec=0;
		tv.tv_usec=0;
		int error= ioctl(RXsockfd, SIOCGSTAMP, &tv);

		//gets the time since the last packet
		int diff_sec= tv.tv_sec - *prev_sec;
		int diff_usec= tv.tv_usec - *prev_usec;
		diff_usec= (diff_usec < 0) ? -1 * diff_usec : diff_usec;
		printf("Time since last packet recv'd: ");
		float tmp= (float)diff_usec / 1000.0;
		tmp += (float)diff_sec * 1000.0;
		printf("%f ms\n", tmp);

		*prev_sec= tv.tv_sec;
		*prev_usec= tv.tv_usec;

		return numbytes;
	}

	printf("Wrong destination MAC: %x:%x:%x:%x:%x:%x\n",
					eh->ether_dhost[0],
					eh->ether_dhost[1],
					eh->ether_dhost[2],
					eh->ether_dhost[3],
					eh->ether_dhost[4],
					eh->ether_dhost[5]);

	return -1;
}

int TX(int sockfd, char *send_buffer, int num_bytes, int *ifindex, uint8_t *src_mac, uint8_t *dest_mac, struct ether_header *eh, int not_wait_for_rx, uint16_t iter_ct, int num_iters){
	int tx_len=0;
	
	//set source and dest host
	memcpy((void*)(eh->ether_shost), (void*)src_mac, ETH_ALEN);
	memcpy((void*)(eh->ether_dhost), (void*)dest_mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP); //ethernet type

	tx_len += sizeof(struct ether_header);
	if(not_wait_for_rx)  send_buffer[tx_len++]= 0;
	else 		     send_buffer[tx_len++]= 1;

	//set done or not done with test
	if(iter_ct == num_iters -1) send_buffer[tx_len++]= 1;
	else			    send_buffer[tx_len++]= 0;

	send_buffer[tx_len++]= (char)(iter_ct);
	send_buffer[tx_len++]= (char)(iter_ct >> 8);
	
	printf("[%d] ",iter_ct);

	if(not_wait_for_rx) printf("Transmitting %dB routing packet\n", num_bytes);
	else 		    printf("Transmitting %dB data packet\n", num_bytes);

	while(tx_len < (num_bytes + sizeof(struct ether_header)) )
		send_buffer[tx_len++] = 2;
	
	struct sockaddr_ll saddr;
	saddr.sll_ifindex = *ifindex; 
	saddr.sll_halen = ETH_ALEN; 

	memcpy((void*)(saddr.sll_addr), (void*)dest_mac, ETH_ALEN);

	/* Send packet */
	if(sendto(sockfd, send_buffer, tx_len, 0, 
		(struct sockaddr*)&saddr, sizeof(struct sockaddr_ll)) < 0)
	    return -1;
	
	return 0;
}

int main(int argc, char *argv[]){

	char ifName[IFNAMSIZ];
	strcpy(ifName, DEFAULT_IF);

	int RXsockfd;
	RXsockfd= initRX(ifName);
	uint8_t buf[BUF_SIZ];
	struct ether_header *eh = (struct ether_header *) buf;
	int *prev_usec= malloc(sizeof(int));
	int *prev_sec= malloc(sizeof(int));
	int tmp;

	int TXsockfd;

	int ifindex;
	uint8_t src_mac[ETH_ALEN];
	uint8_t dest_mac[ETH_ALEN];
	dest_mac[0]= 0xdc;
	dest_mac[1]= 0xa6;
	dest_mac[2]= 0x32;
	dest_mac[3]= 0xf7;
	dest_mac[4]= 0xe7;
	dest_mac[5]= 0x36;

	TXsockfd= initTX(&ifindex, src_mac, ifName);

	char send_buffer[BUF_SIZ];
	memset(send_buffer, 0, BUF_SIZ);
	struct ether_header *TXeh = (struct ether_header *) send_buffer;
	
	//init GPIO 
	GPIOExport(GPIO_PIN);
	GPIOExport(GPIO_PIN_DONE);
	GPIODirection(GPIO_PIN, 1); //set as output
	GPIODirection(GPIO_PIN_DONE, 1); //set as output

	//get cmdline args
	int c; 
	int i=1;
	char *filename= NULL;
	int num_iters= NUM_ITERS;
	while( (c=getopt(argc, argv, "fn")) != -1){
		switch(c){
			case 'f': 
				filename= argv[i+1]; 
				break;
			case 'n': 
				num_iters= atoi(argv[i+2]);
				break;
			default:
				printf("Invalid Arg\n");
				break;
		}
		i++;
	}

	if(filename == NULL){
		printf("must include filename\n");
	       	return -1;
	}
	FILE *file = fopen(filename, "r");

	int not_wait_for_rx= 0;
	int sleepTime= 100;
	int num_bytes= 10;

	char csv_buf[400];
	int iter_ct= 0;
	while(fgets(csv_buf, sizeof(csv_buf), file) && iter_ct < num_iters) {
		const char delim[2] = ",";

		char *data;
		data= strtok(csv_buf, delim);
		not_wait_for_rx= atoi(data);

		data= strtok(NULL, delim);
		sleepTime= atoi(data);

		data= strtok(NULL, delim);
		num_bytes= atoi(data);

		/* For single RX/TX tests
		num_bytes= 80;
		not_wait_for_rx= 1;
		sleepTime= 10; //ms
		*/


		GPIOWrite(GPIO_PIN, 1); //begin data collection
		TX(TXsockfd, send_buffer, num_bytes, &ifindex, src_mac, dest_mac, TXeh, not_wait_for_rx, iter_ct, num_iters); 

		GPIOWrite(GPIO_PIN, 0); //end data collection
		if(!not_wait_for_rx){
			if((tmp= RX(RXsockfd, eh, buf, prev_sec, prev_usec)) < 0) 
				printf("MAC-addr connection error\n");
		}

		usleep(sleepTime);

		iter_ct++;

	}

	GPIOWrite(GPIO_PIN, 0); //end data collection
	GPIOWrite(GPIO_PIN_DONE, 1); //finished parsing the csv
	sleep(1);
	GPIOWrite(GPIO_PIN_DONE, 0); //finished parsing the csv

	GPIOUnexport(GPIO_PIN);
	GPIOUnexport(GPIO_PIN_DONE);
	close(RXsockfd);
	close(TXsockfd);
	return 0;
}

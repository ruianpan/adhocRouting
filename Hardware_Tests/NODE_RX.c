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
#define DEST_MAC4	0xe7 
#define DEST_MAC5	0x36

//other device I believe
#define MY_DEST_MAC0	0xdc
#define MY_DEST_MAC1	0xa6
#define MY_DEST_MAC2	0x32
#define MY_DEST_MAC3	0xf7
#define MY_DEST_MAC4	0xde
#define MY_DEST_MAC5	0x19

#define ETHER_TYPE	0x0800

#define DEFAULT_IF	"wlan0"
#define BUF_SIZ		1024
#define FRAME_HDR_SIZE  14
#define GPIO_PIN	17
#define GPIO_PIN_DONE   27

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

int RX(int RXsockfd, struct ether_header *eh, char *buf, int *prev_sec, int *prev_usec, int *tx_ACK, int *tx_done){
	int numbytes;
	numbytes = recvfrom(RXsockfd, buf, BUF_SIZ, 0, NULL, NULL);
	GPIOWrite(GPIO_PIN, 0); //end data collection

	if (eh->ether_dhost[0] == DEST_MAC0 &&
			eh->ether_dhost[1] == DEST_MAC1 &&
			eh->ether_dhost[2] == DEST_MAC2 &&
			eh->ether_dhost[3] == DEST_MAC3 &&
			eh->ether_dhost[4] == DEST_MAC4 &&
			eh->ether_dhost[5] == DEST_MAC5) {
		size_t tmp1= sizeof(struct ether_header);

		//if recv'd a routing frame don't ACK it back to the tx'ing node
		if(buf[tmp1++]) *tx_ACK= 1; //TX node is waiting for RX to ACK 
		else 	        *tx_ACK= 0;

		if(buf[tmp1++])	 *tx_done= 1;
		else 		 *tx_done= 0;

		uint8_t tmp2= buf[tmp1++];
		uint16_t tmp3= buf[tmp1++];
		printf("[%u] ", tmp2 | (tmp3 << 8));
		printf("Frame of %lu data bytes recv'd ", numbytes - FRAME_HDR_SIZE);

		
		struct timeval tv;
		tv.tv_sec=0;
		tv.tv_usec=0;
		int error= ioctl(RXsockfd, SIOCGSTAMP, &tv);

		//gets the time since the last packet
		int diff_sec= tv.tv_sec - *prev_sec;
		int diff_usec= tv.tv_usec - *prev_usec;
		diff_usec= (diff_usec < 0) ? -1 * diff_usec : diff_usec;

		float tmp= (float)diff_usec / 1000.0;
		tmp += (float)diff_sec * 1000.0;
		printf("+%f ms\n", tmp);

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

int TX(int sockfd, char *send_buffer, int num_bytes, int *ifindex, uint8_t *src_mac, uint8_t *dest_mac, struct ether_header *eh){
	printf("Transmitting %d data bytes\n", num_bytes);
	int tx_len=0;
	
	//set source and dest host
	memcpy((void*)(eh->ether_shost), (void*)src_mac, ETH_ALEN);
	memcpy((void*)(eh->ether_dhost), (void*)dest_mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP); //ethernet type

	tx_len += sizeof(struct ether_header);
	while(tx_len < (num_bytes + sizeof(struct ether_header)) )
		send_buffer[tx_len++] = 1;
	
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
	dest_mac[4]= 0xde;
	dest_mac[5]= 0x19;

	TXsockfd= initTX(&ifindex, src_mac, ifName);
	char send_buffer[BUF_SIZ];
	memset(send_buffer, 0, BUF_SIZ);
	struct ether_header *TXeh = (struct ether_header *) send_buffer;

	//init GPIO 
	GPIOExport(GPIO_PIN);
	GPIOExport(GPIO_PIN_DONE);
	GPIODirection(GPIO_PIN, 1); //set as output
	GPIODirection(GPIO_PIN_DONE, 1); //set as output

	int num_bytes= 0; //just an ACK to the other RPi
	int tx_ACK= 0;
	int tx_done= 0;
	while(1){
		if((tmp= RX(RXsockfd, eh, buf, prev_sec, prev_usec, &tx_ACK, &tx_done)) < 0) 
			printf("MAC-addr connection error\n");

		//ACK back to TX node if recv'd data packet
		if(tx_ACK) TX(TXsockfd, send_buffer, num_bytes, &ifindex, src_mac, dest_mac, TXeh); 
		GPIOWrite(GPIO_PIN, 1); //begin data collection

		if(tx_done) break;
	}

	GPIOWrite(GPIO_PIN, 0); //end data collection
	GPIOWrite(GPIO_PIN_DONE, 1);
	sleep(1);
	GPIOWrite(GPIO_PIN_DONE, 0);

	GPIOUnexport(GPIO_PIN_DONE);
	GPIOUnexport(GPIO_PIN);
	close(RXsockfd);
	close(TXsockfd);
	return 0;
}

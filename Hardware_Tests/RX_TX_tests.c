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

#define DEST_MAC0	0xdc
#define DEST_MAC1	0xa6
#define DEST_MAC2	0x32
#define DEST_MAC3	0xf7
#define DEST_MAC4	0xde
#define DEST_MAC5	0x19

//other device I believe
#define MY_DEST_MAC0	0x00
#define MY_DEST_MAC1	0x00
#define MY_DEST_MAC2	0x00
#define MY_DEST_MAC3	0x00
#define MY_DEST_MAC4	0x00
#define MY_DEST_MAC5	0x00

#define ETHER_TYPE	0x0800

#define DEFAULT_IF	"wlan0"
#define BUF_SIZ		1024

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


int initTX(struct ifreq *if_idx, struct ifreq *if_mac, char *ifName){
	int sockfd;

	/* Open RAW socket to send on */
	if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
	    perror("socket");
	    exit(1);
	}

	/* Get the index of the interface to send on */
	memset(if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx->ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFINDEX, if_idx) < 0)
	    perror("SIOCGIFINDEX");

	/* Get the MAC address of the interface to send on */
	memset(if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac->ifr_name, ifName, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFHWADDR, if_mac) < 0)
	    perror("SIOCGIFHWADDR");

	return sockfd;
}

int RX(int RXsockfd, struct ether_header *eh, char *buf, int *prev_sec, int *prev_usec){
	int numbytes;
	numbytes = recvfrom(RXsockfd, buf, BUF_SIZ, 0, NULL, NULL);
	printf("Frame of %lu bytes recv'd\n", numbytes);

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
		printf("Time since last packet recv'd: %d.%d sec\n", diff_sec, diff_usec);
		*prev_sec= tv.tv_sec;
		*prev_usec= tv.tv_usec;

		return numbytes;
	}

	return -1;
}

int TX(int sockfd, char *send_buffer, struct ifreq *if_idx, struct ifreq *if_mac, struct ether_header *eh){
	int tx_len=0;
	
	//set source host
	eh->ether_shost[0] = ((uint8_t *)&(if_mac->ifr_hwaddr.sa_data))[0];
	eh->ether_shost[1] = ((uint8_t *)&(if_mac->ifr_hwaddr.sa_data))[1];
	eh->ether_shost[2] = ((uint8_t *)&(if_mac->ifr_hwaddr.sa_data))[2];
	eh->ether_shost[3] = ((uint8_t *)&(if_mac->ifr_hwaddr.sa_data))[3];
	eh->ether_shost[4] = ((uint8_t *)&(if_mac->ifr_hwaddr.sa_data))[4];
	eh->ether_shost[5] = ((uint8_t *)&(if_mac->ifr_hwaddr.sa_data))[5];

	//set destination host
	eh->ether_dhost[0] = MY_DEST_MAC0;
	eh->ether_dhost[1] = MY_DEST_MAC1;
	eh->ether_dhost[2] = MY_DEST_MAC2;
	eh->ether_dhost[3] = MY_DEST_MAC3;
	eh->ether_dhost[4] = MY_DEST_MAC4;
	eh->ether_dhost[5] = MY_DEST_MAC5;

	/* Ethertype field */
	eh->ether_type = htons(ETH_P_IP);
	tx_len += sizeof(struct ether_header);

	/*Packet Data -- ausfuhllen hier bitte*/
	//append to sendbuf sendbuf[tx_len++] = 1
	
	struct sockaddr_ll socket_address;
	socket_address.sll_ifindex = if_idx->ifr_ifindex; //device idx
	socket_address.sll_halen = ETH_ALEN; //addr len
	/* Destination MAC */
	socket_address.sll_addr[0] = MY_DEST_MAC0;
	socket_address.sll_addr[1] = MY_DEST_MAC1;
	socket_address.sll_addr[2] = MY_DEST_MAC2;
	socket_address.sll_addr[3] = MY_DEST_MAC3;
	socket_address.sll_addr[4] = MY_DEST_MAC4;
	socket_address.sll_addr[5] = MY_DEST_MAC5;

	/* Send packet */
	if (sendto(sockfd, send_buffer, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0){
	    printf("TX failure\n");
	    return -1;
	}
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
	struct ifreq if_idx;
	struct ifreq if_mac;
	TXsockfd= initTX(&if_idx, &if_mac, ifName);
	char send_buffer[BUF_SIZ];
	memset(send_buffer, 0, BUF_SIZ);
	struct ether_header *TXeh = (struct ether_header *) send_buffer;


	while(1){
		if((tmp= RX(RXsockfd, eh, buf, prev_sec, prev_usec)) < 0) break;
		TX(TXsockfd, send_buffer, &if_idx, &if_mac, TXeh);
	}

	close(RXsockfd);
	close(TXsockfd);
	return 0;
}

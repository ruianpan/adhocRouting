/**
 * @file    sockInit.c
 *
 * @brief   Implementation of socket initialization for recieving 
 * 	    and sending frames from/to nodes 
 *
 * @date    May 7th, 2021
 * @author  Arden Diakhate-Palme
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
#include "sockInit.h"

/**@brief Initializes a socket to recieve packets on */
int initRX(char *ifName){
	int sockfd, sockopt;
	if( (sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0){
		perror("listening socket");
		exit(1);
	}

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

/**@brief Initializes a socket to send packets on */
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

	/*IOCTL call to retrieve the hardware index of the interface to send on*/
	if (ioctl(sockfd, SIOCGIFINDEX, &tmp) < 0) perror("SIOCGIFINDEX");
	*ifindex= tmp.ifr_ifindex;

	/*IOCTL call to retrieve the source index of the wireless interface */
	if (ioctl(sockfd, SIOCGIFHWADDR, &tmp) < 0) perror("SIOCGIFHWADDR");
	memcpy((void*)src_mac, (void*)(tmp.ifr_hwaddr.sa_data), ETH_ALEN);

	return sockfd;
}

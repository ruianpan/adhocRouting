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
#include "gpioLib.h"

#define DEST_MAC0	0xdc
#define DEST_MAC1	0xa6
#define DEST_MAC2	0x32
#define DEST_MAC3	0xf7
#define DEST_MAC4	0xe7
#define DEST_MAC5	0x36

#define ETHER_TYPE	0x0800

#define DEFAULT_IF	"wlan0"
#define BUF_SIZ		1024

#define GPIO_PIN	17
#define GPIO_PIN_DONE   27

char sender[INET6_ADDRSTRLEN];
uint8_t buf[BUF_SIZ];

int main(int argc, char *argv[])
{
	int sockfd, sockopt;
	
	/* use default interface */
	char ifName[IFNAMSIZ];
	strcpy(ifName, DEFAULT_IF);

	/* Open PF_PACKET socket, listening for EtherType ETHER_TYPE */
	if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETHER_TYPE))) == -1) {
		perror("listener: socket");	
		return -1;
	}

	struct ifreq ifopts;	/* set promiscuous mode */
	strncpy(ifopts.ifr_name, ifName, IFNAMSIZ-1);
	ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(sockfd, SIOCSIFFLAGS, &ifopts);

	/* Allow the socket to be reused - incase connection is closed prematurely */
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
		perror("setsockopt");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	/* Bind to device */
	if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ifName, IFNAMSIZ-1) == -1)	{
		perror("SO_BINDTODEVICE");
		close(sockfd);
		exit(EXIT_FAILURE);
	}
	/* Header structures */
	struct ether_header *eh = (struct ether_header *) buf;
	struct iphdr *iph = (struct iphdr *) (buf + sizeof(struct ether_header));
	struct udphdr *udph = (struct udphdr *) (buf + sizeof(struct iphdr) + sizeof(struct ether_header));

	struct ifreq if_ip;	/* get ip addr */
	memset(&if_ip, 0, sizeof(struct ifreq));

	struct sockaddr_storage their_addr;
	ssize_t numbytes;
	int ret;

	//initalize GPIO pins
	GPIOExport(GPIO_PIN);
	GPIOExport(GPIO_PIN_DONE);
	GPIODirection(GPIO_PIN, 1); //set as output
	GPIODirection(GPIO_PIN_DONE, 1); //set as output

	while(1){
		printf("Waiting to recvfrom...\n");
		GPIOWrite(GPIO_PIN, 1); //begin data collection
		numbytes = recvfrom(sockfd, buf, BUF_SIZ, 0, NULL, NULL);
		GPIOWrite(GPIO_PIN, 0); //begin data collection

		printf("listener: got packet %lu bytes\n", numbytes);

		/* Check the packet is for me */
		if (eh->ether_dhost[0] == DEST_MAC0 &&
				eh->ether_dhost[1] == DEST_MAC1 &&
				eh->ether_dhost[2] == DEST_MAC2 &&
				eh->ether_dhost[3] == DEST_MAC3 &&
				eh->ether_dhost[4] == DEST_MAC4 &&
				eh->ether_dhost[5] == DEST_MAC5) {

			size_t tmp1= sizeof(struct ether_header);
			tmp1++;
			if(buf[tmp1++])	 break;
			
		} else {
			printf("Wrong destination MAC: %x:%x:%x:%x:%x:%x\n",
							eh->ether_dhost[0],
							eh->ether_dhost[1],
							eh->ether_dhost[2],
							eh->ether_dhost[3],
							eh->ether_dhost[4],
							eh->ether_dhost[5]);
			ret = -1;
			break;
		}

		/* Get source IP */
		((struct sockaddr_in *)&their_addr)->sin_addr.s_addr = iph->saddr;
		inet_ntop(AF_INET, &((struct sockaddr_in*)&their_addr)->sin_addr, sender, sizeof(sender));

		/* UDP payload length */
		ret = ntohs(udph->len) - sizeof(struct udphdr);
		printf("Data %d bytes\n");
	}

	GPIOWrite(GPIO_PIN, 0); //end data collection
	GPIOWrite(GPIO_PIN_DONE, 1);
	printf("done\n");
	sleep(1);
	GPIOWrite(GPIO_PIN_DONE, 0);

	GPIOUnexport(GPIO_PIN_DONE);
	GPIOUnexport(GPIO_PIN);
	close(sockfd);
	return ret;
}
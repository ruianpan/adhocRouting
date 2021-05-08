/**
 * @file    rx_node.c
 *
 * @bried   Records the current consumption of the recieving node, signaling to the 
 *          microcontroller when to record current (for the duration of frame reception). 
 *          Runs test to generate the power consumption models of the RX node and the FWD node
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
#include <errno.h>

#include "gpioLib.h"
#include "sockInit.h"

//GPIO pins for communicating with the Arduino
#define GPIO_DATA   17
#define GPIO_DONE   27

/**@brief Receives frames from a SOCK_RAW socket differentiates routing and data packets, 
 *        detects routing packets, and when to acknowledge frames*/
int RX(int sockfd, char *src_mac, int *prev_sec, int *prev_usec, int *tx_ACK, int *tx_done){

	ssize_t ether_len= 1518;
	uint8_t buf[ether_len];
	memset(buf, 0, ether_len);
	struct ether_header *eh = (struct ether_header *) buf;

	int numbytes;
	if( (numbytes=recvfrom(sockfd, buf, sizeof(struct ether_header), 0, NULL, NULL)) < 0)
		fprintf(stderr, "recvfrom error %d\n", errno);
	GPIOWrite(GPIO_DATA, 0); //end current-consumption data collection

	printf("rx: %lu bytes ", numbytes - sizeof(struct ether_header));

	//check if the ethernet frame is intended for this node
	if (eh->ether_dhost[0] == src_mac[0] && eh->ether_dhost[1] == src_mac[1] &&
	    eh->ether_dhost[2] == src_mac[2] && eh->ether_dhost[3] == src_mac[3] &&
	    eh->ether_dhost[3] == src_mac[3] && eh->ether_dhost[4] == src_mac[4] &&
	    eh->ether_dhost[5] == src_mac[5]){ 

		//recieve the rest of the bytes 
		memset(buf, 0, sizeof(struct ether_header));
		if((numbytes=recvfrom(sockfd, buf, ether_len, 0, NULL, NULL)) < 0)
			fprintf(stderr, "recvfrom error %d\n", errno);

		size_t i= sizeof(struct ether_header);

		//checks if the received frame as a routing packet
		if(buf[i++]) *tx_ACK= 1; 
		else 	        *tx_ACK= 0;

		//checks if routing algo simulation is complete
		if(buf[i++])	 *tx_done= 1;
		else 		 *tx_done= 0;

		//get the line number of simulation
		uint8_t tmp2= buf[i++];
		uint16_t tmp3= buf[i++];
		printf("[%u] ", tmp2 | (tmp3 << 8));
		
		//gets the time since the last packet arrived
		struct timeval tv;
		tv.tv_sec=0;
		tv.tv_usec=0;
		int error= ioctl(sockfd, SIOCGSTAMP, &tv);

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
	printf("Frame intended for other node\n");

	return -1;
}

/**@brief Transmits frames of num_bytes of payload from a SOCK_RAW socket 
 * @param 
 * @return 0 on success, -1 on transmission failure */
int TX(int sockfd, int num_bytes, int *ifindex, uint8_t *src_mac, uint8_t *dest_mac){
	if(!num_bytes)  printf("tx: ACK\n");
	else		printf("tx: %dB data\n");

	char send_buffer[1518];
	memset(send_buffer, 0, 1518);
	struct ether_header *eh = (struct ether_header *) send_buffer;
	
	//set source and dest host
	memcpy((void*)(eh->ether_shost), (void*)src_mac, ETH_ALEN);
	memcpy((void*)(eh->ether_dhost), (void*)dest_mac, ETH_ALEN);
	eh->ether_type= htons(ETH_P_IP); 

	//set payload bytes 
	int tx_len=0;
	tx_len += sizeof(struct ether_header);
	while(tx_len < (num_bytes + sizeof(struct ether_header)) )
		send_buffer[tx_len++] = 1;
	
	//link layer socket 
	struct sockaddr_ll saddr;
	saddr.sll_ifindex= *ifindex; 
	saddr.sll_halen= ETH_ALEN; 
	memcpy((void*)(saddr.sll_addr), (void*)dest_mac, ETH_ALEN);

	/* Send packet */
	if(sendto(sockfd, send_buffer, tx_len, 0, 
		(struct sockaddr*)&saddr, sizeof(struct sockaddr_ll)) < 0)
	    return -1;
	
	return 0;
}

int main(int argc, char *argv[]){
	//set the interface as wireless network card
	char ifName[]= "wlan0";

	int ifindex;
	uint8_t src_mac[ETH_ALEN];
	uint8_t dest_mac[ETH_ALEN];
	dest_mac[0]= 0xdc;
	dest_mac[1]= 0xa6;
	dest_mac[2]= 0x32;
	dest_mac[3]= 0xf7;
	dest_mac[4]= 0xe7;
	dest_mac[5]= 0x36;

	//initialize transmission and reception sockets 
	int TXsockfd, RXsockfd;
	RXsockfd= initRX(ifName);
	TXsockfd= initTX(&ifindex, src_mac, ifName);
	
	//initialize GPIO pins to communicate with MCU
	GPIOExport(GPIO_DATA);
	GPIOExport(GPIO_DONE);
	GPIODirection(GPIO_DATA, 1); //set as output
	GPIODirection(GPIO_DONE, 1); //set as output

	//timing locals for frame arrival 
	int *prev_usec= malloc(sizeof(int));
	int *prev_sec= malloc(sizeof(int));

	int tx_ACK= 0;
	int tx_done= 0;
	int num_bytes= 0; 

	while(1){
		if(num_bytes=RX(RXsockfd, src_mac, prev_sec, prev_usec, &tx_ACK, &tx_done) < 0) 
			printf("MAC-addr connection error\n");

		/* Transmit an ACK back to TX node if recv'd data packet (0 byte payload)*/
		/* FWD node: Transmit the entire frame (entire payload of num_bytes received) back to TX node*/
		if(tx_ACK) TX(TXsockfd, 0, &ifindex, src_mac, dest_mac); 

		GPIOWrite(GPIO_DATA, 1); //begin current-consumption data collection

		if(tx_done) break;
	}

	GPIOWrite(GPIO_DATA, 0); //end data collection
	GPIOWrite(GPIO_DONE, 1);
	sleep(1);
	GPIOWrite(GPIO_DONE, 0);

	GPIOUnexport(GPIO_DONE);
	GPIOUnexport(GPIO_DATA);
	close(RXsockfd);
	close(TXsockfd);
	return 0;
}

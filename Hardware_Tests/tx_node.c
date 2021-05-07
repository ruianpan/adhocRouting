/**
 * @file    tx_node.c
 *
 * @brief   Runs a simulation of a given routing algorithm, signaling to the 
 *          microcontroller when to record current (during frame transmission). 
 *          Runs tests to generate power-consumption model of the TX node
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

#include "gpioLib.h"
#include "sockInit.h"

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

#define ETHER_TYPE	ETH_P_IP

#define DEFAULT_IF	"wlan0"
#define BUF_SIZ		1024
#define FRAME_HDR_SIZE  14
#define GPIO_DATA	17
#define GPIO_DONE   27

//number of lines of csv data to parse
#define NUM_ITERS	700


/**@brief Receives frames from a SOCK_RAW socket*/
int RX_ACK(int sockfd, char *src_mac, int *prev_sec, int *prev_usec){

	ssize_t ether_len= 1518;
	uint8_t buf[ether_len];
	memset(send_buffer, 0, ether_len);
	struct ether_header *eh = (struct ether_header *) buf;

	int numbytes;
	if( (numbytes=recvfrom(sockfd, buf, sizeof(struct ether_header), 0, NULL, NULL)) < 0)
		fprintf(stderr, "recvfrom error %d\n", errno);

	printf("rx: %lu bytes ", numbytes - FRAME_HDR_SIZE);

	//check if the ethernet frame is intended for this node
	if (eh->ether_dhost[0] == src_mac[0] && eh->ether_dhost[1] == src_mac[1] &&
	    eh->ether_dhost[2] == src_mac[2] && eh->ether_dhost[3] == src_mac[3] &&
	    eh->ether_dhost[3] == src_mac[3] && eh->ether_dhost[4] == src_mac[4] &&
	    eh->ether_dhost[5] == src_mac[5]){ 

		struct timeval tv;
		tv.tv_sec=0;
		tv.tv_usec=0;
		int error= ioctl(sockfd, SIOCGSTAMP, &tv);

		//gets the time since the last packet arrived
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

int TX(int sockfd, int num_bytes, int *ifindex, uint8_t *src_mac, uint8_t *dest_mac, int is_routing, uint16_t iter_ct, int num_iters){
	char send_buffer[BUF_SIZ];
	memset(send_buffer, 0, BUF_SIZ);
	struct ether_header *eh = (struct ether_header *) send_buffer;
	
	//set source and dest host MAC addresses
	memcpy((void*)(eh->ether_shost), (void*)src_mac, ETH_ALEN);
	memcpy((void*)(eh->ether_dhost), (void*)dest_mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP); //set ethernet type - IP protocol isn't actually being used

	//notify RX node if the packet is a routing packet
	int tx_len=0;
	tx_len += sizeof(struct ether_header);
	if(is_routing)  send_buffer[tx_len++]= 0;
	else 		send_buffer[tx_len++]= 1;

	//notify the RX node if a test is complete 
	if(iter_ct == num_iters -1) send_buffer[tx_len++]= 1;
	else			    send_buffer[tx_len++]= 0;

	//notify the RX node of the current iteration in the simulation
	send_buffer[tx_len++]= (char)(iter_ct);
	send_buffer[tx_len++]= (char)(iter_ct >> 8);
	
	printf("[%d] ",iter_ct);

	if(is_routing)    printf("tx: %dB routing packet\n", num_bytes);
	else 		  printf("tx: %dB data packet\n", num_bytes);

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

	//get which routing algorithm .csv to run and the number of lines to simulate 
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
		printf("must include parsed simulation file\n");
	       	return -1;
	}
	FILE *file = fopen(filename, "r");

	int is_routing= 0;
	int sleepTime= 100;
	int num_bytes= 10;

	char csv_buf[400];
	int iter_ct= 0;
	while(fgets(csv_buf, sizeof(csv_buf), file) && iter_ct < num_iters) {
		const char delim[2] = ",";

		char *data;
		data= strtok(csv_buf, delim);
		is_routing= atoi(data);

		data= strtok(NULL, delim);
		sleepTime= atoi(data);

		data= strtok(NULL, delim);
		num_bytes= atoi(data);

		/* For single RX/TX tests manually set the num_bytes, sleepTime, and is_routing */

		GPIOWrite(GPIO_DATA, 1); //begin data collection
		TX(sockfd, num_bytes, &ifindex, src_mac, dest_mac, not_wait_for_rx, iter_ct, num_iters); 

		GPIOWrite(GPIO_DATA, 0); //end data collection
		if(!not_wait_for_rx){
			if(RX_ACK(sockfd, src_mac, prev_sec, prev_usec) < 0) 
				printf("MAC-addr connection error\n");
		}

		usleep(sleepTime);
		iter_ct++;
	}

	GPIOWrite(GPIO_DATA, 0); //end data collection
	GPIOWrite(GPIO_DONE, 1); //finish parsing the csv
	sleep(1);
	GPIOWrite(GPIO_DONE, 0); //finish parsing the csv

	GPIOUnexport(GPIO_DATA);
	GPIOUnexport(GPIO_DONE);
	close(RXsockfd);
	close(TXsockfd);
	return 0;
}

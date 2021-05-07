
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

int initL2(struct ifreq *if_idx, struct ifreq *if_mac, char *ifName){
	int sockfd, sockopt;
	if( (sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) < 0){
		perror("listening socket");	
		exit(1);
	}

	/* Set interface to promiscuous mode - do we need to do this every time? */
	struct ifreq ifopts;	/* set promiscuous mode */
	strncpy(ifopts.ifr_name, ifName, IFNAMSIZ-1);
	ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
	ifopts.ifr_flags |= IFF_PROMISC;
	ioctl(sockfd, SIOCSIFFLAGS, &ifopts);

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

int RX_ACK(int RXsockfd, struct ether_header *eh, char *buf, int *prev_sec, int *prev_usec){

	int tmp1;
	tmp1= recvfrom(RXsockfd, buf, 19, 0, NULL, NULL);
	printf("rx: ACK ");

	if (eh->ether_dhost[0] == DEST_MAC0 && eh->ether_dhost[1] == DEST_MAC1 &&
			eh->ether_dhost[2] == DEST_MAC2 && eh->ether_dhost[3] == DEST_MAC3 &&
			eh->ether_dhost[4] == DEST_MAC4 && eh->ether_dhost[5] == DEST_MAC5) {

		if(buf[sizeof(struct ether_header)] == 88) printf(" request ");
		if(buf[sizeof(struct ether_header)] == 44) printf(" data ");

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

		return 0;
	}

	printf("Wrong destination MAC: %x:%x:%x:%x:%x:%x\n", eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2], eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5]);

	return -1;
}
int TX_request(int sockfd, int num_bytes, struct ifreq *if_idx, struct ifreq *if_mac, int is_routing, int num_iters, int iter_ct){

	char send_buffer[BUF_SIZ];
	memset(send_buffer, 0, BUF_SIZ);
	struct ether_header *eh = (struct ether_header *) send_buffer;

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
	eh->ether_type = htons(ETH_P_IP);

	int tx_len=0;
	tx_len+= sizeof(struct ether_header);

	send_buffer[tx_len++]= (char)(num_bytes);
	send_buffer[tx_len++]= (char)(num_bytes >> 8);

	
	struct sockaddr_ll socket_address;
	socket_address.sll_ifindex = if_idx->ifr_ifindex; //device idx
	socket_address.sll_halen = ETH_ALEN; //addr len
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

int TX_data(int sockfd, int num_bytes, struct ifreq *if_idx, struct ifreq *if_mac, int is_routing, int num_iters, int iter_ct){

	char send_buffer[1518];
	memset(send_buffer, 0, 1518);
	struct ether_header *eh = (struct ether_header *) send_buffer;

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

	eh->ether_type = htons(ETH_P_IP);

	int tx_len=0;
	tx_len += sizeof(struct ether_header);

	if(is_routing)  send_buffer[tx_len++]= 1;
	else 		send_buffer[tx_len++]= 0;


	if(iter_ct == num_iters -1) send_buffer[tx_len++]= 1;
	else			    send_buffer[tx_len++]= 0;

	//bytes in addition to the header
	while(tx_len < sizeof(struct ether_header) + 2 + num_bytes) 
		send_buffer[tx_len++] = 88;
	
	struct sockaddr_ll socket_address;
	socket_address.sll_ifindex = if_idx->ifr_ifindex; //device idx

	socket_address.sll_halen = ETH_ALEN; //addr len
	socket_address.sll_addr[0] = MY_DEST_MAC0;
	socket_address.sll_addr[1] = MY_DEST_MAC1;
	socket_address.sll_addr[2] = MY_DEST_MAC2;
	socket_address.sll_addr[3] = MY_DEST_MAC3;
	socket_address.sll_addr[4] = MY_DEST_MAC4;
	socket_address.sll_addr[5] = MY_DEST_MAC5;

	/* Send packet */

	GPIOWrite(GPIO_PIN, 1); //begin data collection

	if (sendto(sockfd, send_buffer, tx_len, 0, (struct sockaddr*)&socket_address, sizeof(struct sockaddr_ll)) < 0){
	    printf("TX failure\n");
	    return -1;
	}

	GPIOWrite(GPIO_PIN, 0); //end data collection
	return 0;
}

int main(int argc, char *argv[]){

	char ifName[IFNAMSIZ];
	strcpy(ifName, DEFAULT_IF);

	uint8_t buf[BUF_SIZ];
	struct ether_header *eh = (struct ether_header *) buf;
	int *prev_usec= malloc(sizeof(int));
	int *prev_sec= malloc(sizeof(int));
	int tmp;

	struct ifreq if_idx;
	struct ifreq if_mac;

	int sockfd;
	sockfd= initL2(&if_idx, &if_mac, ifName);

	
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
			case 'f': //size in bytes
				filename= argv[i+1]; 
				break;
			case 'n': //size in bytes
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

	int is_routing= 0;
	int sleepTime= 100;
	int num_bytes= 10;

	char csv_buf[400];
	int iter_ct= 0;
	int ct= 0;
	while(fgets(csv_buf, sizeof(csv_buf), file) && iter_ct < num_iters) {
		const char delim[2] = ",";

		char *data;
		data= strtok(csv_buf, delim);
		is_routing= atoi(data);

		data= strtok(NULL, delim);
		sleepTime= atoi(data);

		data= strtok(NULL, delim);
		num_bytes= atoi(data);

		num_bytes= ct;
		ct+= 5;

		/*
		if(TX_request(sockfd, num_bytes, &if_idx, &if_mac, is_routing, num_iters, iter_ct) == 0)
			printf("request to TX: %dB of data\n", num_bytes);
		
		RX_ACK(sockfd, eh, buf, prev_sec, prev_usec);
		printf("cleared to TX: %dB of data\n", num_bytes);
		*/

		TX_data(sockfd, num_bytes, &if_idx, &if_mac, is_routing, num_iters, iter_ct);

		//if(!is_routing) RX_ACK(sockfd, eh, buf, prev_sec, prev_usec);

		printf("tx: %dB data\n", num_bytes);

		printf("\n");
		
		iter_ct++;
	}

	GPIOWrite(GPIO_PIN, 0); //end data collection
	GPIOWrite(GPIO_PIN_DONE, 1); //finished parsing the csv
	sleep(1);
	GPIOWrite(GPIO_PIN_DONE, 0); //finished parsing the csv

	GPIOUnexport(GPIO_PIN);
	GPIOUnexport(GPIO_PIN_DONE);
	close(sockfd);
	return 0;
}

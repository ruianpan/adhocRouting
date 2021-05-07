/**
 * @file    sockInit.h
 *
 * @brief   Interface to initialize sockets for raw frame transmision and reception
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

/**@brief Initializes a socket to recieve packets on */
int initRX(char *ifName);

/**@brief Initializes a socket to send packets on */
int initTX(int *ifindex, uint8_t *src_mac, char *ifName);

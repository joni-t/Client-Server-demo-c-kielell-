/* Joni Taipale, 0342424, joni.taipale@lut.fi */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <time.h>
#include <netinet/tcp.h>

int give_multicast_addr(int channel, char* mcast_addr, uint16_t *port); // funktio asettaa annetun kanavan osoitetiedot
int join_channel(int channel, int *sockfd, struct sockaddr_in *mc_chat_addr, size_t *addrlen); // function join to multicast channel
int leave_channel(int sockfd, struct sockaddr_in mc_chat_addr); // function leave from multicast channel
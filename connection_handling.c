/* Joni Taipale, 0342424, joni.taipale@lut.fi */


#include "connection_handling.h"

// funktio asettaa annetun kanavan osoitetiedot
int give_multicast_addr(int channel, char* mcast_addr, uint16_t *port)
{
	// check given channel
	bzero(mcast_addr, 15+1);
	if (channel==1)
	{
		strncpy(mcast_addr, "224.0.0.241", 11);
		*port=11111;
	}
	else if (channel==2)
	{
		strncpy(mcast_addr, "224.0.0.242", 11);
		*port=11112;
	}
	else if (channel==3)
	{
		strncpy(mcast_addr, "224.0.0.243", 11);
		*port=11113;
	}
	else
	{
		printf("Invalid channel number: %i. Channels are 1, 2 and 3.\n", channel);
		return -1;
	}
	return 0;
}

// function join to multicast channel
int join_channel(int channel, int *sockfd, struct sockaddr_in *mc_chat_addr, size_t *addrlen)
{
	char mcast_addr[15+1];
	uint16_t port;
	struct ip_mreq mreq;	// address structure for setsockopt function
	int loop=0;

	// Create the socket
	if ((*sockfd = socket(AF_INET,SOCK_DGRAM,0)) < 0)
	{
		perror("Error in opening sockfd for multicast");
		return -1;
	}

	// selvitetään kanavan osoitetiedot
	if (give_multicast_addr(channel, mcast_addr, &port) <0)
	{
		return -1;
	}

	// Initialize the address members
	*addrlen=sizeof(*mc_chat_addr); // size of the address structure
	bzero(mc_chat_addr,*addrlen); // Pad with nulls
	(*mc_chat_addr).sin_family = AF_INET; // using IPv4 protocol
	(*mc_chat_addr).sin_addr.s_addr = inet_addr(mcast_addr); // multicast group address
	(*mc_chat_addr).sin_port=htons(port); // port number to use

	// Try to listen port
	if (bind(*sockfd,(struct sockaddr *)mc_chat_addr,*addrlen) < 0)
	{
		perror("Error in bind multicast port");
		close(*sockfd); // Error
		return -1;
	}

	// joining to the channel
	mreq.imr_multiaddr.s_addr = (*mc_chat_addr).sin_addr.s_addr;
	mreq.imr_interface.s_addr = INADDR_ANY;
	if (setsockopt(*sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) <0)
	{
		perror("Error in joining to multicast group");
		return -1;
	}
	if (setsockopt(*sockfd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, 1) <0)	// set loop-back off
	{
		perror("Error in setting loop-back off");
		return -1;
	}

	printf("You are joined in channel %i\n", channel);
	
	return 0;
}

// function leave from multicast channel
int leave_channel(int sockfd, struct sockaddr_in mc_chat_addr)
{
	struct ip_mreq mreq;	// address structure for setsockopt function

	mreq.imr_multiaddr.s_addr = mc_chat_addr.sin_addr.s_addr;
	mreq.imr_interface.s_addr = INADDR_ANY;

	if (setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) <0)
	{
		perror("Error in leaving from the multicast group");
		return -1;
	}
	close(sockfd);
	return 0;
}

// funktio lähettää pelitiedustelun valitulle multicastryhmäkanavalle
int inquire(int channel, int inquiry_channel, int mcastsockfd, struct sockaddr_in mc_chat_addr, char* message_send_udp, uint16_t rnd_udp_port)
{
	char mcast_addr[15+1];
	uint16_t port;
	size_t addrlen;
	uint16_t len=0;

	// jos kanava on eri kuin mihin ryhmään ollaan liitytty, joudutaan asettamaan uuden kanava tiedot (mutta ei pysyvästi)
	if (channel != inquiry_channel)
	{
		// Create the new socket
		if ((mcastsockfd = socket(AF_INET,SOCK_DGRAM,0)) < 0)
		{
			perror("Error in opening sockfd for inquiry");
			return -1;
		}
		if (give_multicast_addr(inquiry_channel, mcast_addr, &port) <0)
		{
			return -1;
		}
		// Initialize the address members
		//addrlen=sizeof(mc_chat_addr); // size of the address structure
		bzero(&mc_chat_addr,addrlen); // Pad with nulls
		mc_chat_addr.sin_family = AF_INET; // using IPv4 protocol
		mc_chat_addr.sin_addr.s_addr = inet_addr(mcast_addr); // multicast group address
		mc_chat_addr.sin_port=htons(port); // port number to use
	}
	addrlen=sizeof(mc_chat_addr); // size of the address structure

	// pack the message
	len=inquiry_message_packing(message_send_udp, rnd_udp_port);
	// send the message
	if (sendto(mcastsockfd, message_send_udp, len, 0, (struct sockaddr *)&mc_chat_addr,addrlen) < 0)
	{
		perror("Error in sending inguire message");
		return -1;
	}

	return 0;
}
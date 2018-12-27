/* Joni Taipale, 0342424, joni.taipale@lut.fi */

#include "gameserver_main.h"
#include "gameserver.h"

// main program check that the parameters given properly
int main(int argc, char *argv[])
{
	uint16_t udpport;		// udp port
	uint16_t tcpport;		// tcp port
	int channel;		// multicast group channel
	int i;

	// is all parameters given correctly?
	int udp_ok=0;
	int tcp_ok=0;
	int channel_ok=0;

	if (argc == 7) // 6 nmb of paramters (-ptcp <tcp port> -pudp <udp port> -c <1|2|3>) must given
	{
		// check one parameter at time
		for(i=1; i<6; i=i+2)
		{
			if (strncmp(argv[i], "-ptcp", 5) == 0) // flag: tcp port
			{
				tcp_ok=1;
				tcpport=atoi(argv[i+1]);
			}
			else if (strncmp(argv[i], "-pudp", 5) == 0) // udp port
			{
				udp_ok=1;
				udpport=atoi(argv[i+1]);
			}
			else if (strncmp(argv[i], "-c", 2) == 0) // channel number
			{
				channel=atoi(argv[i+1]);
				channel_ok=1;
			}
			else // unknown parameter
			{
				printf("Invalid parameter(s). Usage:\n ./gameserver -ptcp <tcp port> -pudp <udp port> -c <1|2|3>\n");
				return -1;
			}
		}
		if (udp_ok==0 || tcp_ok==0 || channel_ok==0) // is all parameters given correctly?
		{	// wrong parameters
			printf("Invalid parameter(s). Usage:\n ./gameserver -ptcp <tcp port> -pudp <udp port> -c <1|2|3>\n");
			return -1;
		}
		if(gameserver(udpport, tcpport, channel)==0) // execute gameserver
		{
			printf("Game server exited with success.\n");
		}
    		else
    		{
      			printf("Errors with game server.\n");
      			return -1;
		}
	}
	else
	{
		printf("Invalid amount of arguments. Usage:\n ./gameserver -ptcp <tcp port> -pudp <udp port> -c <1|2|3>\n");
		return -1;
	}
	
	return 0;
}

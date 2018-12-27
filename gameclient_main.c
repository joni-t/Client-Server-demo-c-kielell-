/* Joni Taipale, 0342424, joni.taipale@lut.fi */

#include "gameclient_main.h"
#include "gameclient.h"
#include "message_handling.h"

// main program check that the parameters given properly
int main(int argc, char *argv[])
{
	int i;
	int channel;			// multicast group channel
	char nick[MAXLENGTHNICK+1];	// user's nick name
	
	// is all parameters given correctly?
	int channel_ok=0;
	int nick_ok=0;
	
	// set all fields to 0
	bzero(nick, MAXLENGTHNICK+1);

	if (argc == 5) // 4 nmb of paramters (-n <nickname> -c <1|2|3>) must given
	{
		// check one parameter at time
		for(i=1; i<4; i=i+2)
		{
			if (strcmp(argv[i], "-c") == 0) // channel number
			{
				channel_ok=1;
				channel=atoi(argv[i+1]);
			}
			else if (strcmp(argv[i], "-n") == 0) // nick name
			{
				nick_ok=1;
				strncpy(nick, argv[i+1], MAXLENGTHNICK);
			}
			else // tuntematon parametri
			{
				printf("Invalid parameter(s). Usage:\n ./gameclient -n <nickname> -c <1|2|3>");
				return -1;
			}
		}
		if (channel_ok==0 || nick_ok==0) // is all parameters given correctly?
		{	// wrong parameters
			printf("Invalid parameter(s). Usage:\n ./gameclient -n <nickname> -c <1|2|3>\n");
			return -1;
		}
		else if(gameclient(nick, channel)==0) // execute game client
		{
			printf("Game client exited with success.\n");
		}
    		else
    		{
      			printf("Errors with game client.\n");
      			return -1;
    		}
	}
	else
	{
		printf("Invalid amount of arguments. Usage:\n ./gameclient -n <nickname> -c <1|2|3>\n");
		return -1;
	}
	
	return 0;
}
/* Joni Taipale, 0342424, joni.taipale@lut.fi */
#include "gameclient.h"
#include "message_handling.h"
#include "connection_handling.h"

serv_node_pointer servers;	// linked list of adverted servers
char userinput[MAXLINE];
int msg_len_inp=0;
fd_set rset;		// for select function
char message_send_udp[MAXLINE];		// message buffer for send udp message
char message_rec_udp[MAXLINE];		// message buffer for receiving udp message
uint16_t dgram_size_recv;
uint16_t dgram_size_send;
char message_send_tcp[MAXLINE];		// message buffer for send tcp stream
char message_rec_tcp[MAXLINE];		// message buffer for receiving tcp stream
uint16_t bytes_sended_tcp=0, bytes_left_send_tcp=0;
uint16_t bytes_recved_tcp=0, bytes_left_recv_tcp=0;
struct sockaddr_in serv_addr_udp; // address structure for server's udp connection
struct sockaddr_in serv_addr_tcp; // address structure for server's tcp connection
struct sockaddr_in mc_chat_addr; // address structure for multicast group connection
struct sockaddr_in server_reply_addr;	// address structure for servers reply
struct sockaddr_in client_addr;	// address structure for client
size_t addrlen; // size of the address structure
int source_addrlen;
int udpsockfd;	// socket's descriptor for udp connection (for inquire message)
int udpgamesockfd; // socket's descriptor for udp connection (for game messages)
int tcpsockfd;	// socket's descriptor for tcp connection
int mcastsockfd;	// socket's descriptor for multicast group connection
int maxsockfd=0;
unsigned int last_time_send_msg=0;	// time of last sending udp message
struct timeval tempo;
uint32_t user_id;

// Prepare and start client with given parameters// parameters: nick and multicast group channel number
// function wait commands from user: /channel, /inquire, /listgames and /join
int gameclient(char *nick, int channel)
{
	int new_channel=0;
	int inquiry_channel=0;
	int rnd_udp_port=0;
	int game_choice=0;
	time_t now;
	servers=NULL;

	time(&now);
	srand(now);
	rnd_udp_port = (rand() % 2001) + 2000; // random udp port number between 2000-4000

	// Create the UDP socket
	// t‰m‰ on inquire viesti‰ varten ja bindataan
	if ((udpsockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Error in creating UDP socket to client");
		return -1;
	}
	// Create the UDP socket
	// t‰m‰ on peliviestej‰ varten ja ei bindata
	if ((udpgamesockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Error in creating UDP game socket to client");
		return -1;
	}
	printf("UDP sockets created to client.\n");

	// Initialize the address members
	addrlen=sizeof(client_addr); // size of the address structure
    	bzero(&client_addr,addrlen); // Pad with nulls
    	client_addr.sin_family = AF_INET; // using IPv4 protocol
	client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	client_addr.sin_port=htons(rnd_udp_port); // server udp port number

	// Try to bind UDP socket
	if (bind(udpsockfd, (struct sockaddr *)&client_addr,addrlen) < 0)
	{	
		perror("Error in bind UDP");
      		close(udpsockfd);
      		return -1;
    	}

	// join to multicast group
	if (join_channel(channel, &mcastsockfd, &mc_chat_addr, &addrlen) <0)
	{	// error happened
		return -1;
	}

	maxsockfd=mcastsockfd;
	source_addrlen=addrlen; // t‰m‰ pit‰‰ asettaa ett‰ struct luetaan oikein recvfrom:lta

	printf("Game client is started now and wait user commands.\n");

	// Handling messages from user (/channel, /inquire, /listgames and /join) and receiving game advert from multicast group
	while(1)
	{
		// initialize select parameters
		FD_ZERO(&rset); // set all bits to zero
		FD_SET(0, &rset); // set bit for STD_IN
		FD_SET(mcastsockfd, &rset); // set bit for multicast fd
		FD_SET(udpsockfd, &rset); // set bit for udp fd
		if (Call_Select(&rset, NULL, 0, maxsockfd+1) >0)
		{
			if(is_fd_ready(0, &rset))
			{	// read user input
				if ((msg_len_inp = read_std_in(userinput, MAXLINE)) <0)
					return -1;
				// vaihdetaan kanavaa
				if(strncmp(userinput, "/channel", 8) ==0)
				{	// join multicast channel
					new_channel=atoi(&userinput[9]);
					// check channel
					if (new_channel>3 || new_channel<1)
					{
						printf("Invalid channel_number. Possible channels are between 1 and 3.\n");
					}
					else if (new_channel != channel)
					{	// leave first from old channel
						if (leave_channel(mcastsockfd, mc_chat_addr) <0)
						{
							return -1;
						}
						// join to new channel
						channel=new_channel;
						if (join_channel(channel, &mcastsockfd, &mc_chat_addr, &addrlen) <0)
						{	// error happened
							return -1;
						}
						if (maxsockfd<mcastsockfd)
							maxsockfd=mcastsockfd;
					}
					else
					{
						printf("You are already joined in channel %i\n", channel);
					}
				}
				// l‰hetet‰‰n pelitiedustelu
				else if(strncmp(userinput, "/inquire", 8) ==0)				
				{	// inquiries for a game	
					inquiry_channel=atoi(&userinput[9]);
					// check channel
					if (inquiry_channel>3 || inquiry_channel<1)
					{
						printf("Invalid channel_number. Possible channels are between 1 and 3.\n");
					}
					else if (inquire(channel, inquiry_channel, mcastsockfd, mc_chat_addr, message_send_udp, rnd_udp_port) <0)
					{	
						return -1;
					}				
				}
				// listataan pelit
				else if(strncmp(userinput, "/listgames", 10) ==0)				
				{
					printf("List of games:\nChoice\tip\t\t\t\tudp port\ttcp port\n\n");
					list_of_games(&servers, 1);
				}
				// rekisterˆidyt‰‰n palvelimelle
				else if(strncmp(userinput, "/join", 5) ==0)				
				{
					game_choice=atoi(&userinput[6]);
					if (join_in_game(&servers, 1, game_choice, nick) ==0)
					{	// rekisterˆinti onnistui
						if (gameclient_in_game(nick) <0)
						{	// virhe tapahtunut. suljetaan socketit ja poistutaan virhekoodilla
							close(udpsockfd);
							close(udpgamesockfd);
							close(tcpsockfd);
							close(mcastsockfd);
							// puretaan pelilista
							while(servers)
								remove_serv_node(&servers);
							return -1;
						}
					}
				}
				
				// suljetaan ohjelma
				else if(strncmp(userinput, "/quit", 5) ==0)				
				{
					close(udpsockfd);
					close(udpgamesockfd);
					close(tcpsockfd);
					close(mcastsockfd);
					// puretaan pelilista
					while(servers)
						remove_serv_node(&servers);
					return 0;
				}
				else
				{
					printf("Unknown command. Possible commands in unregistered is:\n/channel <1|2|3>, /inquire <1|2|3>, /listgames, /join <game id on list> and quit.\n");
				}
			}		

			// read game advert from multicast group channel
			else if(is_fd_ready(mcastsockfd, &rset))
			{
				bzero(message_rec_udp, MAXLINE);
				dgram_size_recv = recvfrom(mcastsockfd, message_rec_udp, MAXLINE, 0, (struct sockaddr*) &server_reply_addr, &source_addrlen);
				if (check_message_type(message_rec_udp, dgram_size_recv) ==ADVERT)
				{	// k‰sitell‰‰n viesti -> lis‰t‰‰n mainos linkitettyyn listaan
					handling_advert_msg(message_rec_udp, server_reply_addr, &servers);
				}
			}

			// read udp unicast message
			else if(is_fd_ready(udpsockfd, &rset))
			{
				bzero(message_rec_udp, MAXLINE);
				dgram_size_recv = recvfrom(udpsockfd, message_rec_udp, MAXLINE, 0, (struct sockaddr*) &server_reply_addr, &source_addrlen);
				if (check_message_type(message_rec_udp, dgram_size_recv) ==ADVERT)
				{	// k‰sitell‰‰n viesti -> lis‰t‰‰n mainos linkitettyyn listaan
					handling_advert_msg(message_rec_udp, server_reply_addr, &servers);
				}
			}
		}
	}
	return 0;
}

// funktio k‰sittelee peli‰ koskevat toiminnot (asiakas on rekisterˆitynyt)
int gameclient_in_game(char *nick)
{
	uint16_t current_question_id=0;
	uint8_t answer_choice=0;

	// Handling messages from user (/leave, /answer, message) and receiving game advert from multicast group
	while(1)
	{
		// initialize select parameters
		FD_ZERO(&rset); // set all bits to zero
		FD_SET(0, &rset); // set bit for STD_IN
		FD_SET(mcastsockfd, &rset); // set bit for multicast fd
		FD_SET(udpgamesockfd, &rset); // set bit for udp game fd
		FD_SET(tcpsockfd, &rset); // set bit for tcp fd
		if (Call_Select(&rset, NULL, 0, maxsockfd+1) >0)
		{
			if(is_fd_ready(0, &rset))
			{	// read user input
				if ((msg_len_inp = read_std_in(userinput, MAXLINE)) <0)
					return -1;
				// selvitet‰‰n onko komento vai chat-viesti
				if (strncmp(userinput, "/", 1) ==0)
				{	// komento
					// vastataan kysymykseen
					if(strncmp(userinput, "/answer", 7) ==0)				
					{	
						answer_choice=atoi(&userinput[8]);
						if (send_answer(current_question_id, answer_choice) <0)
						{	
							return -1;
						}
						continue; // jatketaan silmukkaa alusta				
					}

					// poistutaan pelist‰
					if(strncmp(userinput, "/leave", 6) ==0)				
					{	// l‰hetet‰‰n poistumisviesti
						dgram_size_send = goodbye_message_packing(message_send_udp, user_id);
						if (sendto(udpgamesockfd, message_send_udp, dgram_size_send, 0, (struct sockaddr *)&serv_addr_udp,addrlen) < 0)
						{
							perror("Error in sending answerring message");
							return -1;
						}
						// suljetaan tcp pelipalvelimelle
						close(tcpsockfd);
						printf("You leaved from game.\n");
						return 0; // poistutaan toiseen valikkoon
					}

					else
					{
						printf("Unknown command. Possible commands in game is:\n/leave, /answer <answer option id>, <message to be sent>.\n");
					}
				}
				// chat viesti (TCP)
				else if (bytes_left_send_tcp==0) // kunhan edellinen viesti on l‰hetetty kokonaan
				{	// pakataan viesti valmiiksi
					bytes_left_send_tcp = chat_from_client_message_packing(message_send_tcp, user_id, userinput);
					bytes_sended_tcp=0;
				}
				else
				{
					printf("Last chat message not sended. Try again.\n");
				}
			}

			// read udp unicast game message
			if(is_fd_ready(udpgamesockfd, &rset))
			{
				bzero(message_rec_udp, MAXLINE);
				dgram_size_recv = recvfrom(udpgamesockfd, message_rec_udp, MAXLINE, 0, (struct sockaddr*) &server_reply_addr, &source_addrlen);
				// kysymysviesti
				if (check_message_type(message_rec_udp, dgram_size_recv) ==QUESTION)
				{	// k‰sitell‰‰n viesti -> tulostetaan kysymys ja talletetaan id
					handling_question_msg(message_rec_udp, dgram_size_recv, &current_question_id);
				}
				// infoviesti
				else if (check_message_type(message_rec_udp, dgram_size_recv) ==INFO)
				{	// vain tulostetaan
					printf("%s\n", unpack_info_msg(message_rec_udp));
				}
				// pisteet viesti
				else if (check_message_type(message_rec_udp, dgram_size_recv) ==SCORES)
				{	// tulostetaan pisteet
					unpack_scores_msg(message_rec_udp, nick);
				}
				// voittajaviesti
				else if (check_message_type(message_rec_udp, dgram_size_recv) ==WINNER)
				{	// vain tulostetaan
					printf("And the winner is %s.\n", &message_rec_udp[1]);
				}
			}

			// read game advert from multicast group channel
			if(is_fd_ready(mcastsockfd, &rset))
			{
				bzero(message_rec_udp, MAXLINE);
				dgram_size_recv = recvfrom(mcastsockfd, message_rec_udp, MAXLINE, 0, (struct sockaddr*) &server_reply_addr, &source_addrlen);
				if (check_message_type(message_rec_udp, dgram_size_recv) ==ADVERT)
				{	// k‰sitell‰‰n viesti -> lis‰t‰‰n mainos linkitettyyn listaan
					handling_advert_msg(message_rec_udp, server_reply_addr, &servers);
				}
			}

			// read chat message (TCP)
			if(is_fd_ready(tcpsockfd, &rset))
			{
				if (bytes_recved_tcp==0)
				{	// new message
					bytes_left_recv_tcp=MAXLINE-1;	// don't know yet size of message
					bzero(message_rec_tcp, MAXLINE);
				}
				if (recvpart(tcpsockfd, message_rec_tcp, &bytes_recved_tcp, &bytes_left_recv_tcp) <0)	// receive part of message
				{
					printf("Error in receiving chat message.\n");
					return -1;
				}
				if (bytes_left_recv_tcp == 0)	// is the whole message readed
				{	// puretaan viesti ja tulostetaan se
					if (message_rec_tcp[0]=='C') // pit‰‰ olla tyyppi‰ chat
					{
						unpack_chat_from_server_msg(message_rec_tcp);
					}
					bytes_recved_tcp=0;
				}
			}
		}

		// l‰hetet‰‰n TCP:ll‰
		if (bytes_left_send_tcp>0)
		{
			if (sendpart(tcpsockfd, message_send_tcp, &bytes_sended_tcp, &bytes_left_send_tcp) < 0)
			{
				printf("Error in sending Chat message to server.\n");
				return -1;
			}
		}
	}


	return 0;
}

// funktio purkaa mainosviestin ja lis‰‰ palvelimen tiedot linkitettyyn listaan
void handling_advert_msg(char* msg_ptr, struct sockaddr_in server_reply_addr, serv_node_pointer *p_serv)
{
	char ip[15+1];
	uint16_t udpport, tcpport;
	int counter=0;	// laskuri laskee mainosten m‰‰r‰n. jos menee yli, vanhin poistetaan

	//puretaan viesti
	bzero(ip, 15+1);
	strncpy(ip, (char *)inet_ntoa(server_reply_addr.sin_addr), 15);
	unpack_advert_msg(msg_ptr, &udpport, &tcpport);
	// lis‰t‰‰n linkitettyyn listaan
	counter++;
	add_serv_node(p_serv, ip, udpport, tcpport, &counter);
	// jos on yli 10 mainosta, poistetan vanhimpia
	while(counter>10)
	{
		remove_serv_node(&(*p_serv));
		counter--;
	}
}

// add new server/advert node
void add_serv_node(serv_node_pointer *p, char* ip, uint16_t udpport, uint16_t tcpport, int *counter)
{
	if(!(*p))
	{	// are we end of the list -> add here new node
		if((*p = (serv_node_pointer) malloc(sizeof(serv_node_type))) == (serv_node_pointer)NULL)
		{
			perror("malloc"); //exit(1);
		}
		(*p)->next = NULL;

		// asetaan j‰senien tiedot
		bzero((*p)->ip, 15+1);
		strncpy((*p)->ip, ip, 15+1);
		(*p)->udpport=udpport;
		(*p)->tcpport=tcpport;
	}	
	else
	{
		// jos on dublikaatti, poistetaan vanhempi mainos (uusi lis‰t‰‰n listan loppuun)
		if (strncmp((*p)->ip, ip, 15)==0 && (*p)->udpport==udpport && (*p)->tcpport==tcpport)
		{
			remove_serv_node(&(*p));
			add_serv_node(&(*p), ip, udpport, tcpport, counter); // siirryt‰‰n poistettua seuraavaan
		}
		else
		{
			// go to next node
			(*counter)++;
			add_serv_node(&(*p)->next, ip, udpport, tcpport, counter);
		}
	}
}

// poistaa serverin/mainoksen solmun
void remove_serv_node(serv_node_pointer *p)
{
	serv_node_pointer p_help;

	if((*p))
	{	
		// liitet‰‰n edellinen ja seuraava toisiinsa ja vapautetaan muisti poistettavan solmun osalta
		p_help=(*p);
		(*p)=(*p)->next;
		free(p_help);
		p_help=NULL;
	}
}

// listaa mainostetut pelit
void list_of_games(serv_node_pointer *p, int choice)
{
	if((*p))
	{
		printf("%i.\t%s\t\t\t%i\t\t%i\n", choice++, (*p)->ip, (*p)->udpport, (*p)->tcpport);
		if((*p)->next != NULL)
		{
			list_of_games(&(*p)->next, choice); // move to next game
		}
	}
}

// liittyy, rekisterˆi, ottaa yhteyden valitulle pelipalvelimelle
// palauttaa nollan mik‰li kaikki menee putkeen, tai -1 mik‰li jotain menee pieleen
int join_in_game(serv_node_pointer *p, int counter, int game_choice, char *nick)
{
	uint16_t len=0;

	if((*p))
	{
		if (counter==game_choice)
		{	// liityt‰‰n t‰h‰n peliin
			printf("Try to registering...\n");
			
			// Initialize the address members for udp
			addrlen=sizeof(serv_addr_udp); // size of the address structure
    			bzero(&serv_addr_udp,addrlen); // Pad with nulls
    			serv_addr_udp.sin_family = AF_INET; // using IPv4 protocol
			serv_addr_udp.sin_addr.s_addr = inet_addr((*p)->ip); // server ip address
    			serv_addr_udp.sin_port=htons((*p)->udpport); // server port number

			// pack the registering message
			len=registering_message_packing(message_send_udp, nick);
			// send the registering message
			if (sendto(udpgamesockfd, message_send_udp, len, 0, (struct sockaddr *)&serv_addr_udp,addrlen) < 0)
			{
				perror("Error in sending registering message");
				return -1;
			}

			// asetetaan time-out 3s ja odotellaan vastaus
			gettimeofday(&tempo, NULL);
			last_time_send_msg = (unsigned int)tempo.tv_sec;
			FD_ZERO(&rset); // set all bits to zero
			FD_SET(udpgamesockfd, &rset); // set bit for udp fd
			while (((unsigned int)tempo.tv_sec-last_time_send_msg) <TIME_OUT && Call_Select(&rset, NULL, 0, maxsockfd+1) ==0)
			{	// kunnes time-out tulee t‰yteen tai socket:iin alkaa tulla tavaraa
				gettimeofday(&tempo, NULL);
				FD_ZERO(&rset); // set all bits to zero
				FD_SET(udpgamesockfd, &rset); // set bit for udp fd
			}
			// luetaan paketti, mik‰li tuli
			if(Call_Select(&rset, NULL, 0, maxsockfd+1)>0 && is_fd_ready(udpgamesockfd, &rset))
			{
				bzero(message_rec_udp, MAXLINE);
				dgram_size_recv = recvfrom(udpgamesockfd, message_rec_udp, MAXLINE, 0, (struct sockaddr*) &server_reply_addr, &source_addrlen);
				if (check_message_type(message_rec_udp, dgram_size_recv) ==CONNECTED)
				{	// k‰sitell‰‰n viesti -> talletetaan user_id
					unpack_connected_msg(message_rec_udp, &user_id);
				}
				else if (check_message_type(message_rec_udp, dgram_size_recv) ==ERROR)
				{	// k‰sitell‰‰n viesti -> tulostetaan tyyppi ja lis‰viesti
					unpack_error_msg(message_rec_udp);
					return -1;
				}
			}
			else
			{
				printf("Time out. Server not found.\n");
				return -1;
			}
			printf("Registering accepted. Try open TCP connection...\n");			

			// avataan TCP-yhteys
			if ((tcpsockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
			{
				perror("Error in creating TCP socket to client.\n");
				return -1;
			}
			if (maxsockfd<tcpsockfd)
				maxsockfd=tcpsockfd;

			// Initialize the address members
			addrlen=sizeof(serv_addr_tcp); // size of the address structure
    			bzero(&serv_addr_tcp,addrlen); // Pad with nulls
    			serv_addr_tcp.sin_family = AF_INET; // using IPv4 protocol
			serv_addr_tcp.sin_addr.s_addr = inet_addr((*p)->ip); // server ip address
    			serv_addr_tcp.sin_port=htons((*p)->tcpport); // server port number

			// Connect to server
			if(connect(tcpsockfd, (struct sockaddr *)&serv_addr_tcp, addrlen) <0)
			{
				perror("Server not found.\n");
				return -1;
			}
			
			// l‰hetet‰‰n connect viesti, jolla palvelin yhdist‰‰ yhteyden user_id:hen
			printf("Send 'connect message.\n");
			bzero(userinput, MAXLINE);
			strncpy(userinput, "connect", 7);
			bytes_left_send_tcp=chat_from_client_message_packing(message_send_tcp, user_id, userinput);
			bytes_sended_tcp=0;
			while (bytes_left_send_tcp>0)  // l‰hetet‰‰n koko viesti
			{
				if (sendpart(tcpsockfd, message_send_tcp, &bytes_sended_tcp, &bytes_left_send_tcp) < 0)
				{
					printf("Error in sending 'connect' message to server.\n");
					return -1;
				}
			}
			printf("Tcp connection opened with success.\n");

			return 0;
		}
		else if((*p)->next != NULL) // ei t‰h‰n peliin -> koitetaan seuraavaa
		{
			counter++;
			return join_in_game(&(*p)->next, counter, game_choice, nick); // move to next game
		}
		else
		{
			printf("Game not found for choice: %i.\n", game_choice);
			return -1;
		}
	}
}

// funktio purkaa kysymysviestin, tulostaa sen ja tallettaa kysymyksen id:n
void handling_question_msg(char* msg_ptr, uint16_t dgram_size_recv, uint16_t *current_question_id)
{
	char question[MAXLINE];
	uint8_t nmb_of_options;			// vastausvaihtoehtojen m‰‰r‰
	char options[MAXLINE];		// vastausvaihtoehdot erotettuna '\0' merkill‰
	int counter=0;
	int i=0;
	int len=0;

	//puretaan viesti
	bzero(question, MAXLINE);
	bzero(options, MAXLINE);
	unpack_question_msg(msg_ptr, dgram_size_recv, current_question_id, question, &nmb_of_options, options);

	// tulostetaan kysymys
	printf("Question: %s\n", question);
	// ja vastausvaihtoehdot
	for (counter=0; counter<nmb_of_options; counter++)
	{
		len=strlen(&options[i]);
		printf("\t %i. %s\n", counter+1, &options[i]);
		i += len+1;
	}
}

// funktio pakkaa vastausviestin, l‰hett‰‰ sen ja odottaa kuittauksen (tai time-out)
// palauttaa nollan mik‰li kaikki menee putkeen, tai -1 mik‰li jotain menee pieleen
int send_answer(uint16_t current_question_id, uint8_t answer_choice)
{
	uint16_t len=0;
	uint32_t user_id_in_ok_msg=0;
	uint16_t question_id_in_ok_msg=0;

	// pack the answerring message
	len=answerring_message_packing(message_send_udp, user_id, current_question_id, answer_choice);
	// send the answerring message
	if (sendto(udpgamesockfd, message_send_udp, len, 0, (struct sockaddr *)&serv_addr_udp,addrlen) < 0)
	{
		perror("Error in sending answerring message");
		return -1;
	}
	// asetetaan time-out 3s ja odotellaan vastaus
	gettimeofday(&tempo, NULL);
	last_time_send_msg = (unsigned int)tempo.tv_sec;
	FD_ZERO(&rset); // set all bits to zero
	FD_SET(udpgamesockfd, &rset); // set bit for udp fd
	while (((unsigned int)tempo.tv_sec-last_time_send_msg) <TIME_OUT && Call_Select(&rset, NULL, 0, maxsockfd+1) ==0)
	{	// kunnes time-out tulee t‰yteen tai socket:iin alkaa tulla tavaraa
		gettimeofday(&tempo, NULL);
		FD_ZERO(&rset); // set all bits to zero
		FD_SET(udpgamesockfd, &rset); // set bit for udp fd
	}
	// luetaan paketti, mik‰li tuli
	if(Call_Select(&rset, NULL, 0, maxsockfd+1)>0 && is_fd_ready(udpgamesockfd, &rset))
	{
		bzero(message_rec_udp, MAXLINE);
		dgram_size_recv = recvfrom(udpgamesockfd, message_rec_udp, MAXLINE, 0, (struct sockaddr*) &server_reply_addr, &source_addrlen);
		if (check_message_type(message_rec_udp, dgram_size_recv) ==OK)
		{	// k‰sitell‰‰n viesti -> tarkistetaan ett‰ k‰ytt‰j‰id ja kysymysid on oikea ja ilmoitetaan siit‰ pelaajalle
			unpack_ok_msg(message_rec_udp, &user_id_in_ok_msg, &question_id_in_ok_msg);
		}
		else if (check_message_type(message_rec_udp, dgram_size_recv) ==ERROR)
		{	// k‰sitell‰‰n viesti -> tulostetaan tyyppi ja lis‰viesti
			unpack_error_msg(message_rec_udp);
			return 0;  // t‰m‰n takia ei tarvitse poistua virhekoodilla
		}
	}
	else
	{
		printf("Time out. Server not found.\n");
		return -1;
	}

	// Verrataan paketin mukana tulleita k‰ytt‰j‰id:‰ ja kysymysid:‰
	if (user_id_in_ok_msg==user_id && question_id_in_ok_msg==current_question_id)
		printf("Answerring accepted.\n");
	else
		printf("Server sended ok message but with incorrect user id and/or question id.\n");
	return 0;
}

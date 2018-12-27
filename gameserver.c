/* Joni Taipale, 0342424, joni.taipale@lut.fi */

#include "gameserver.h"
#include "message_handling.h"
#include "connection_handling.h"

cli_node_pointer clients;	// linked list of clients which sended request message
cli_node_pointer multicast;     // yhden alkion "linkitetty lista" multicast viestien lähettämiseen
cli_node_pointer winner;	// osoittaa voittaja clienttiin, mikäli on voittaja
msg_node_pointer messages;      // linkitetty lista lähteville viesteille. yksi solmu per viesti. viesti voidan lähettää useille asiakkaille
qstn_node_pointer questions;	// linkitetty lista kysymyksille (ja vastauksille)
qstn_node_pointer current_question;  // pointteri osoittaa käynnissä olevaan kysymykseen
uint16_t udpport, tcpport;
int maxsockfd;
unsigned int game_start_time=0;	// käynnissä olevan pelin aloitusaika
unsigned int last_time_redused_points=0;
unsigned int time_out_counter=0;  // TCP:n 'connect' viestiä varten
int nmb_of_questions=0;
uint16_t current_question_id=1110; // juokseva luku kysymyksien id:ksi
uint8_t points=10;			// pisteet oikeasta vastauksesta (vähennetään 2 pistettä joka 20s)
char sending_msg_buf[MAXLINE];  // väliaikainen bufferi. tähän muodostetaan joitain viestejä, mutta itse lähetys tapahtuu eri bufferista
uint16_t sending_len=0;		// joidenkin lähtevien viestien pituus lasketaan tähän
uint16_t chat_msg_len=0;
char chat_msg[MAXLINE];       // chat viestin viestiosa

// Prepare and start server with given parameters
// parameters: server udp and tcp port number and channel number
int gameserver(uint16_t p_udpport, uint16_t p_tcpport, int channel)
{
	int mcastsockfd; // socket's descriptor for UDP connections (multicast)
	int udpsockfd;	// socket's descriptor for UDP connections (unicast)
	int listenfd;	// socket's descriptor for listening new TCP connections
  	size_t addrlen; // size of the address structure
  	struct sockaddr_in serv_addr; // server address structure
	struct sockaddr_in mc_chat_addr; // address structure for multicast group
	int filefd=0;
	char question_buf[MAXLINE];	// luetaan kysymykset tiedostosta
	int len=0;
	int i, j, k;
	char help_buf[MAXLINE];
	int count;

	// kysymyksien tiedostosta lukua varten
	char question[MAXLINE];
	uint8_t nmb_of_options;
	uint8_t correct_answ_opt;
	char options[MAXLINE];

	clients=NULL;	// linked list of connected clients
	multicast=NULL; // vain yksi alkio. sisältää tiedot multicartosoitteeseen lähettämiseen
	winner=NULL;	// kilpailun voittaja
	messages=NULL;  // lähtevien viestien jono
	questions=NULL; // kilpailukysymykset ja vastaukset
	current_question=NULL;  // käynnissä oleva kysymys
	udpport=p_udpport;
	tcpport=p_tcpport;

	// check port numbers
	if (!(udpport > 1024 && udpport < 65001 && tcpport > 1024 && tcpport < 65001)) // if invalid port number(s)
	{
		if(udpport<1025 || tcpport<1025)
		{
			printf("You can't use systems ports between 1-1024. Usage:\n ./gameserver -ptcp <tcp port> -pudp <udp port> -c <1|2|3>\n");
			return -1;
		}
		if(udpport>64999 || tcpport>64999)
		{
			printf("Invalid port number. Choose something between 1025 - 65000. Usage:\n ./gameserver -ptcp <tcp port> -pudp <udp port> -c <1|2|3>\n");
			return -1;
		}
	}

	printf("Try to start game server...\n");
	
	// Create the socket for UDP
	if ((udpsockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
	{
		perror("Error in creating udp socket to server");
		return -1;
	}
	printf("UDP socket created to server.\n");
	
	// Initialize the address members
	addrlen=sizeof(serv_addr); // size of the address structure
	bzero(&serv_addr,addrlen); // Pad with nulls
	serv_addr.sin_family = AF_INET; // using IPv4 protocol
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port=htons(udpport); // server udp port number

	// Try to bind UDP socket
	if (bind(udpsockfd, (struct sockaddr *)&serv_addr,addrlen) < 0)
	{	
		perror("Error in bind UDP");
      		close(udpsockfd);
      		return -1;
    	}

	// Create the socket for listening TCP connects
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		perror("Error in creating socket to listen TCP connections");
		close(udpsockfd);
		close(listenfd);
		return -1;
	}
	printf("Socket for listening TCP connections created to server.\n");

	// Initialize the address members
	addrlen=sizeof(serv_addr); // size of the address structure
	bzero(&serv_addr,addrlen); // Pad with nulls
	serv_addr.sin_family = AF_INET; // using IPv4 protocol
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port=htons(tcpport); // server tcp port number
	// Try to bind TCP listening socket
	if (bind(listenfd, (struct sockaddr *)&serv_addr,addrlen) < 0)
	{	
		perror("Error in bind TCP");
		close(listenfd);
		close(udpsockfd);
		return -1;
	}

	// Try to listen TCP
	if (listen(listenfd, 1024) < 0)
	{	
		perror("Error in listening");
		close(listenfd);
		close(udpsockfd);
		return -1;
	}

	// join to multicast group
	if (join_channel(channel, &mcastsockfd, &mc_chat_addr, &addrlen) <0)
	{	// error happened
		close(udpsockfd);
		close(listenfd);
		close(mcastsockfd);
		return -1;
	}
	add_cli_node(&multicast, INQUIRE, 0, 0, NULL, mc_chat_addr, addrlen); // lisätään linkitettyyn listaan (listaan ei tule muita kuin tämä yksi solmu). tämä lisää myös ensimmäiset mainosviestit

	// luetaan kysymykset "tietokannasta" (tekstitiedostosta)
	if ((filefd = open("Questions_and_answers.dat", O_RDONLY, "w")) <0)
	{
		perror("Error in opening file.\n");
		return -1;
	}
	bzero(question_buf, MAXLINE);
	len = read(filefd, question_buf, MAXLINE);
	// lisätään kysymykset bufferista linkitettyyn listaan
	i=0;
	j=0;
	while (j<len && question_buf[j]!='|') // haetaa nerotinmerkki
		j++;
	while(j<len)
	{
		j++;
		i = j;
		while (j<len && question_buf[j]!='|') // haetaa nerotinmerkki
			j++;
		// luetaan vastausvaihtoehtojen määrä
		bzero(help_buf, MAXLINE);
		strncpy(help_buf, &question_buf[i], j-i);
		nmb_of_options=atoi(help_buf);
		
		j++;
		i = j;
		while (j<len && question_buf[j]!='|') // haetaa nerotinmerkki
			j++;
		// luetaan oikea vastausvaihtoehto
		bzero(help_buf, MAXLINE);
		strncpy(help_buf, &question_buf[i], j-i);
		correct_answ_opt=atoi(help_buf);

		j++;
		i = j;
		while (j<len && question_buf[j]!='|') // haetaan erotinmerkki
			j++;
		// luetaan kysymys
		bzero(question, MAXLINE);
		strncpy(question, &question_buf[i], j-i);

		// luetaan vastausvaihtoehdot
		k=0;
		bzero(options, MAXLINE);
		for (count=0; count<nmb_of_options; count++)
		{
			j++;
			i = j;
			while (j<len && question_buf[j]!='|') // haetaa nerotinmerkki
				j++;			
			strncpy(&options[k], &question_buf[i], j-i);
			k += (j-i+1);
		}

		// lisätään tiedot linkitettyyn listaan
		add_qstn_node(&questions, question, nmb_of_options, correct_answ_opt, options);
		nmb_of_questions++;

		j++;
		while (j<len && question_buf[j]!='|') // haetaa nerotinmerkki
			j++;
	}
	close(filefd);

	// tarkistetaan, että kysymyksiä löytyy
	if (nmb_of_questions==0)
	{
		printf("Question database is empty.\n");
		close(udpsockfd);
		close(listenfd);
		close(mcastsockfd);
		return -1;
	}

	printf("Server is now ready to waiting messages from client.\n");

	// Move to function that handles traffic betbeen client and server
	// also handles command by user
	if (gameserver_connected(mcastsockfd, udpsockfd, listenfd, addrlen) < 0)
	{	// Error happaned
		// suljetaan resurssit ja siivotaan muisti
		close(udpsockfd);
		close(listenfd);
		close(mcastsockfd);
		while(clients != NULL)
		{
			remove_cli_node(&clients);	// remove client
		}
		remove_cli_node(&multicast);		// poistetaan multicast clientti
		while(messages != NULL)
		{
			remove_msg_node(&messages);	// remove message
		}
		while(questions != NULL)
		{
			remove_qstn_node(&questions);	// remove question
		}
		return -1;
	}

	// suljetaan resurssit ja siivotaan muisti
	close(udpsockfd);
	close(listenfd);
	close(mcastsockfd);
	while(clients != NULL)
	{
		remove_cli_node(&clients);	// remove client
	}
	remove_cli_node(&multicast);		// poistetaan multicast clientti
	while(messages != NULL)
	{
		remove_msg_node(&messages);	// remove message
	}
	while(questions != NULL)
	{
		remove_qstn_node(&questions);	// remove question
	}
	return 0;
}

// function that handles traffic between client and server, also handles command by user
int gameserver_connected(int mcastsockfd, int udpsockfd, int listenfd, int addrlen)
{
	int connfd;	// fd of new TCP connect
	fd_set rset;		// for select function
	int max_fd=mcastsockfd;
	int msg_len_inp;
	char userinput[MAXLINE];
	uint16_t msg_len_recv;
	char message_recv[MAXLINE];		// message buffer for receive message
	uint16_t dgramlen; 	// size of received datagram
	struct sockaddr_in source_addr; // address structure for others
	size_t source_addrlen = addrlen;
	int select_value;	// tell which one is ready for receiving
	unsigned int last_time_send_advert=0;	// time of last advert
	struct timeval tempo;
	uint16_t clients_udp_port;
	char nick[MAXLENGTHNICK+1];	// nick of new client
	uint32_t next_user_id=111111;	// juokseva luku uusien pelaajien id:ksi
	int game_in_open=0;		// ilmaisee onko peli käynnissä
	time_t now;	// kysymyksen arpomista varten
	int rnd_qstn_nbm=0; // arvottavan kysymyksen paikka listassa
	// vastausviestin sisältö
	uint32_t user_id=0;
	uint16_t question_id=0;
	uint8_t answer_choice=0;
	uint16_t bytes_recved_tcp=0, bytes_left_recv_tcp=0;
	
	maxsockfd=mcastsockfd;
	source_addrlen=addrlen;
	gettimeofday(&tempo, NULL);
	last_time_send_advert = (unsigned int)tempo.tv_sec; //laskuri lähtee nollasta ja ekat ajastetut viestit lisätään määräajan kuluttua (ekat on lisätty jo)

	// ikuinen silmukka. hoitelee serverin toimintoja kuten viestien lähettämistä ja vastaanottamista sekä pelin ylläpito
	while(1)
	{
		// initialize select parameters
		FD_ZERO(&rset); // set all bits to zero
		FD_SET(0, &rset); // set bit for STD_IN
		FD_SET(mcastsockfd, &rset); // set bit for multicast fd
		FD_SET(udpsockfd, &rset); // set bit for udp fd
		FD_SET(listenfd, &rset); // set bit for udp fd
		set_tcp_cli_fdset(&clients, &rset);	// set bit of all connected tcp clients
		if (Call_Select(&rset, NULL, 0, maxsockfd+1) >0)
		{
			if(is_fd_ready(0, &rset))
			{	// read user input
				if ((msg_len_inp = read_std_in(userinput, MAXLINE)) <0)
					return -1;
				// suljetaan ohjelma
				if(strncmp(userinput, "/quit", 5) ==0)				
				{
					return 0;
				}
			}

			// luetaan peliä koskevat udp viestit (rekisteröinti, vastaus ja poistumisviesti)
			if(is_fd_ready(udpsockfd, &rset))
			{
				// rekisteröinti
				bzero(message_recv, MAXLINE);
				dgramlen = recvfrom(udpsockfd, message_recv, MAXLINE, 0, (struct sockaddr*) &source_addr, &source_addrlen);
				if (check_message_type(message_recv, dgramlen) ==REGISTER)
				{	// käsitellään viesti -> lisätään uusi rekisteröitynyt asiakas (lisää myös kuittausviestin)
					unpack_register_msg(message_recv, nick);
					add_cli_node(&clients, REGISTER, 0, next_user_id++, nick, source_addr, source_addrlen);
				}
				// vastaus (pelin pitää olla käynnissä)
				else if (game_in_open==1 && check_message_type(message_recv, dgramlen) ==ANSWER)
				{	// käsitellään viesti -> tarkistetaan tiedot ja talletetaan vastaus ja laskee pisteet riippuen kuinka kauan on mennyt aikaa (lisää myös kuittausviestin ja ilmoituksen kaikille rekisteröidyille asiakkaille)
					unpack_answer_msg(message_recv, &user_id, &question_id, &answer_choice);
					check_answer(&clients, user_id, question_id, answer_choice, source_addr, source_addrlen);
				}
				// poistumisviesti
				else if (check_message_type(message_recv, dgramlen) ==GOODBYE)
				{	// käsitellään viesti -> poistetaan asiakas linkitetystä listasta (sulkee myös yhteydet)
					unpack_goodbye_msg(message_recv, &user_id);
					remove_client_by_user_id(&clients, user_id);
				}
			}

			// luetaan inquire-viestit multicast ryhmästä
			if(is_fd_ready(mcastsockfd, &rset))
			{
				bzero(message_recv, MAXLINE);
				dgramlen = recvfrom(mcastsockfd, message_recv, MAXLINE, 0, (struct sockaddr*) &source_addr, &source_addrlen);
				if (check_message_type(message_recv, dgramlen) ==INQUIRE)
				{	// käsitellään viesti -> lisätään uusi rekisteröimätön asiakas (lisää myös mainosviestin)
					unpack_inquire_msg(message_recv, &clients_udp_port);
					source_addr.sin_port=htons(clients_udp_port); // client udp port number
					add_cli_node(&clients, INQUIRE, 0, 0, NULL, source_addr, source_addrlen);
				}
			}

			// otetaan vastaan uusi TCP yhteys
			if(is_fd_ready(listenfd, &rset))
			{	// new TCP connect from client
				if ((connfd=accept(listenfd, (struct sockaddr *)&source_addr, &source_addrlen)) <0)
				{
					printf("Error in accepting new TCP connect.\n");	// just ignore
				}
				else  // odotetaan connect viestiä, jolla yhdistetään yhteys user_id:hen
				{	
					gettimeofday(&tempo, NULL);
					time_out_counter=(unsigned int)tempo.tv_sec;
					bytes_left_recv_tcp=MAXLINE-1;	// don't know yet size of message
					bytes_recved_tcp=0;
					bzero(message_recv, MAXLINE);
					while (((unsigned int)tempo.tv_sec-time_out_counter) <2 && bytes_left_recv_tcp>0) // (annetaan 2 sekuntia aikaa)
					{
						gettimeofday(&tempo, NULL);
						if (recvpart(connfd, message_recv, &bytes_recved_tcp, &bytes_left_recv_tcp) <0)	// receive part of message
						{
							printf("Error in receiving 'connect' message.\n");
							return -1;
						}
					}
					if (bytes_left_recv_tcp==0 && message_recv[0]=='C') // viesti tullut kokonaan ja on tyyppiä chat
					{	// jos koko viesti tuli, yhdistetään yhteys user_id:n perusteella asiakkaaseen
						unpack_chat_from_client_msg(message_recv, &chat_msg_len, &user_id, chat_msg);
						connect_tcp_conn_and_client_by_user_id(&clients, connfd, user_id);
						if (maxsockfd<connfd)
							maxsockfd=connfd;
					}
					else
					{
						close(connfd);
						bytes_left_recv_tcp=0;
					}
				}
			}

			// read tcp message (chat) parts of clients
			read_tcp_msg(&clients, &rset);
		}

		// muodostetaan mainosviestit, mikäli aikaa on mennyt riittävästi
		gettimeofday(&tempo, NULL);
		if (((unsigned int)tempo.tv_sec-last_time_send_advert) >ADVERT_TIME_INTERVAL)
		{
			add_msg_node(&messages, &multicast, ADVERT, ONLY_TO_ONE);
			last_time_send_advert=(unsigned int)tempo.tv_sec;
		}

		// käynnistetään uusi peli, mikäli peli ei ole käynnissä ja on rekisteröitynyt käyttäjä
		gettimeofday(&tempo, NULL);
		if (game_in_open==0 && check_registered_clients(&clients)==1)
		{
			game_start_time=(unsigned int)tempo.tv_sec;  // pelin käynnistysaika talteen
			last_time_redused_points=game_start_time;    // pisteiden vähennys aika
			game_in_open=1;
			points=10;
			winner=NULL;
			// arvotaan kysymys listasta
			time(&now);
			srand(now);	// alustetaan satunnaislukugeneraattori
			rnd_qstn_nbm = rand() % nmb_of_questions+1;	//satunnaisluku välillä 1...nmb_of_questions
			// haetaan kysymyksen osoite ja lähetetään kysymys rekisteröityneille asiakkaille
			if ((current_question = get_qstn_node(&questions, 1, rnd_qstn_nbm)) ==NULL)
			{	// kysymystä ei löytynyt -> suljetaan
				return -1;
			}
			add_msg_node(&messages, &clients, QUESTION, TO_REGISTERED_USERS);
			printf("New game started.\n");
			printf("Player get %u points of correct answer.\n", points);
		}

		// suljetaan peli, mikäli peli on kännissä ja aikaa on mennyt tarpeeksi -> lähetetään pisteet ja mahdollinen voittajailmoitus
		if (game_in_open==1 && ((unsigned int)tempo.tv_sec-game_start_time) >GAME_TIME)
		{	// lasketaan pisteet ja nollataan vastaus seuraavaa kierrosta varten
			// lähetetään kokonaispistetilanne rekisteröidyille käyttäjille
			// jos löytyy voittaja, ilmoitus rekisteröidyille ja pisteiden nollaus
			printf("Close current game.\n");
			game_in_open=0;
			calculate_points_and_send(&clients, 0, 3);
			if (winner!=NULL)
			{	// meillä on voittaja -> lähetetään ilmoitus ja nollataan kokonaispisteet
				bzero(sending_msg_buf, MAXLINE);
				strncpy(sending_msg_buf, "W", 1);
				strncpy(&sending_msg_buf[1], winner->nick, strlen(winner->nick));
				add_msg_node(&messages, &clients, WINNER, TO_REGISTERED_USERS);
				set_points_to_zero(&clients);
				printf("And the winner is %s.\n", winner->nick);
			}
		}

		// vähennetään oikeasta saavia pisteitä 20s välein 2 pistettä
		if (game_in_open==1 && ((unsigned int)tempo.tv_sec-last_time_redused_points) >19)
		{
			points -= 2;
			printf("Player get now only %u points of correct answer.\n", points);
			last_time_redused_points=(unsigned int)tempo.tv_sec;
		}
		
		// lähetetään viestejä
		send_msg_to_client(&multicast, mcastsockfd);  // multicast ryhmäviestit
		send_msg_to_client(&clients, udpsockfd);	// unicast viestit: UDP ja TCP
	}

	return 0;
}

// lisää uuden asiakkaan. parametrina annetaan viestin tyyppi, kummassa yhteydessä asiakas lisätään (mainospyyntö tai rekisteröityminen) ja tämän perusteella muodostetaan paluuviesti. tarkistaa myös ettei pelaajien maksimimäärää ylitetä
void add_cli_node(cli_node_pointer *p, uint16_t msg_type, int registered_counter, uint32_t id, char *nick, struct sockaddr_in p_client_addr, socklen_t p_cli_addr_len)
{
	if(!(*p))
	{	// are we end of the list -> add here new node
		if((*p = (cli_node_pointer) malloc(sizeof(cli_node_type))) == (cli_node_pointer)NULL)
		{
			perror("malloc"); //exit(1);
		}
		(*p)->next = NULL;

		// asetaan jäsenien tiedot
		(*p)->id=id;
		bzero((*p)->nick, MAXLENGTHNICK+1);
		if (nick!=NULL)
			strncpy((*p)->nick, nick, MAXLENGTHNICK);
		(*p)->answer=0;
		(*p)->total_points=0;
		(*p)->new_points=0;
		(*p)->tcpsockfd=-1;       // asetetaan myöhemmin, kun client on luonut tcpyhteyden
		(*p)->client_addr=p_client_addr;
		(*p)->cli_addr_len=p_cli_addr_len;
		bzero((*p)->message_rec, MAXLINE);
		(*p)->p_msg = NULL; // clienttin oma lähtevien viestien jono
		(*p)->bytes_recv_ed=0;
		(*p)->bytes_left_rec=0;

		// lisätään asiakkaalle kuittausviesti riippuen kumman asiakas lähetti
		if (msg_type == INQUIRE)
		{
			add_msg_node(&messages, &(*p), ADVERT, ONLY_TO_ONE);
		}
		else if (msg_type == REGISTER)
		{
			if (registered_counter<MAXUSERS)
			{	// rekisteröinti hyväksytään, mikäli käyttäjiä ei ole liikaa
				add_msg_node(&messages, &(*p), CONNECTED, ONLY_TO_ONE);
			}
			else
			{	// liikaa käyttäjiä -> lähetään virhe ja poistetaan nick (muuttuu rekisteröimättömäksi asiakkaaksi)
				add_msg_node(&messages, &(*p), ERROR_SERVER_IS_FULL, ONLY_TO_ONE);
				bzero((*p)->nick, MAXLENGTHNICK+1);
			}
		}
	}	
	else
	{
		// tarkistetaan ettei nick ole jo käytössä
		if (msg_type == REGISTER && strncmp((*p)->nick, nick, MAXLENGTHNICK) ==0)
		{	// virheviesti asiakkaalle
			add_msg_node(&messages, &(*p), ERROR_NICKNAME_ALREADY_IN_USE, ONLY_TO_ONE);
		}
		else
		{	// go to next node
			if (strlen((*p)->nick) >0)
				registered_counter++;  // laskee rekisteröityneet asiakkaat
			add_cli_node(&(*p)->next, msg_type, registered_counter, id, nick, p_client_addr, p_cli_addr_len);
		}
	}
}

// lisää viestin joko kaikille tai vain yhdelle clientille
void add_msg_node(msg_node_pointer *p, cli_node_pointer *p_client, uint16_t msg_type, int whom)
{
	if(!(*p))
	{	// are we end of the list -> add here new node
		if((*p = (msg_node_pointer) malloc(sizeof(msg_node_type))) == (msg_node_pointer)NULL)
		{
			perror("malloc"); //exit(1);
		}
		(*p)->next = NULL;

		// asetaan jäsenien tiedot
		// viestin pakkaaminen ja protokolla
		if (msg_type==ADVERT)
		{
			(*p)->length=advert_message_packing((*p)->msg, udpport, tcpport);
			(*p)->protocol=UDP;
		}
		else if (msg_type==CONNECTED)
		{
			(*p)->length=connected_message_packing((*p)->msg, (*p_client)->id);
			(*p)->protocol=UDP;
		}
		// error messages
		else if (msg_type==ERROR_INVALID_MESSAGE_ID || msg_type==ERROR_ANSWER_OUT_OF_RANGE || msg_type==ERROR_QUESTION_ALREADY_ANSWERED || msg_type==ERROR_INVALID_USER_ID || msg_type==ERROR_NICKNAME_ALREADY_IN_USE || msg_type==ERROR_SERVER_IS_FULL || msg_type==ERROR_UNKNOWN_ERROR_SITUATION)
		{
			(*p)->length=error_message_packing((*p)->msg, msg_type);
			(*p)->protocol=UDP;
		}
		else if (msg_type==QUESTION)
		{
			(*p)->length=question_message_packing((*p)->msg, ++current_question_id, current_question->question, current_question->nmb_of_options, current_question->options);
			(*p)->protocol=UDP;
		}
		else if (msg_type==OK)
		{
			(*p)->length=ok_message_packing((*p)->msg, (*p_client)->id, current_question_id);
			(*p)->protocol=UDP;
		}
		else if (msg_type==INFO)
		{
			(*p)->length = info_message_packing((*p)->msg, sending_msg_buf);
			(*p)->protocol=UDP;
		}
		else if (msg_type==SCORES)
		{
			// paketti on jo pakattu valmiiksi ja valmis lähetettäväksi
			bzero((*p)->msg, MAXLINE);
			memcpy((*p)->msg, sending_msg_buf, MAXLINE);
			(*p)->length = sending_len;
			(*p)->protocol=UDP;
		}
		else if (msg_type==WINNER)
		{
			// paketti on jo pakattu valmiiksi ja valmis lähetettäväksi
			bzero((*p)->msg, MAXLINE);
			strncpy((*p)->msg, sending_msg_buf, MAXLINE);
			(*p)->length = strlen(sending_msg_buf)+1;
			(*p)->protocol=UDP;
		}
		else if (msg_type==CHAT)
		{
			// paketti on jo pakattu valmiiksi ja valmis lähetettäväksi
			bzero((*p)->msg, MAXLINE);
			memcpy((*p)->msg, sending_msg_buf, MAXLINE);
			(*p)->length = sending_len;
			(*p)->protocol=TCP;
		}

		(*p)->unfinished_clients=0;
		// lisätään viesti valituille clienteille (kaikille tai yhdelle)
		add_msg_to_client(&(*p_client), &(*p), whom);
	}	
	else
	{	// go to next node
		add_msg_node(&(*p)->next, &(*p_client), msg_type, whom);
	}
}

// lisää (linkittää) viestin clientille. parametrilla päätetään, linkitetäänkö viesti kaikille clienteille vai vain yhdelle
void add_msg_to_client(cli_node_pointer *p_cli, msg_node_pointer *p_msg, int whom)
{
	if((*p_cli))
	{
		// lisätään viesti clientille (voidaan vaatia, että käyttäjä on rekisteröitynyt)
		if (whom!=TO_REGISTERED_USERS || whom==TO_REGISTERED_USERS && strlen((*p_cli)->nick)>0) 
			add_clients_own_msg_node(&(*p_cli)->p_msg, &(*p_msg));
		if((*p_cli)->next && whom!=ONLY_TO_ONE)
		{
			add_msg_to_client(&(*p_cli)->next, &(*p_msg), whom); // move to next client
		}
	}
}

// lisää clientille solmun, joka sisältää osoitteen varsinaiseen viestiin ja pitää sisällään viestin lähetyksen tilan
void add_clients_own_msg_node(clients_own_msg_node_pointer *p, msg_node_pointer *p_msg)
{
	if(!(*p))
	{	// are we end of the list -> add here new node
		if((*p = (clients_own_msg_node_pointer) malloc(sizeof(clients_own_msg_node_type))) == (clients_own_msg_node_pointer)NULL)
		{
			perror("malloc"); //exit(1);
		}
		(*p)->next = NULL;

		// asetaan jäsenien tiedot
		(*p)->p_msg=&(*p_msg);
		(*p)->bytes_sended=0;
		(*p)->bytes_left=(*p_msg)->length;
		(*p_msg)->unfinished_clients++;	// kasvatetaan laskuria, joka pitää kirjaa, onko viesti lähetetty kaikille
	}	
	else
	{	// go to next node
		add_clients_own_msg_node(&(*p)->next, &(*p_msg));
	}
}

// funktio lähettää vanhimman viestin (tai sen osan) clientille ja siirtyy seuraavaan clienttiin
// viesti poistetaan (clientilta), kun lähetys saatu loppuun
void send_msg_to_client(cli_node_pointer *p_cli, int udpsockfd)
{
	msg_node_pointer *p_msg;

	if((*p_cli)) // jos client on olemassa
	{
		if ((*p_cli)->p_msg) // jos on viesti tai useampia
		{	// toimitaan viestin protokollan mukaan
			p_msg=(*p_cli)->p_msg->p_msg;  // osoite viestin osoittimeen
			
			// UDP viesti
			if ((*p_msg)->protocol ==UDP)
			{
				if (sendto(udpsockfd, (*p_msg)->msg, (*p_msg)->length, 0, (struct sockaddr *)&((*p_cli)->client_addr), (*p_cli)->cli_addr_len) < 0)
				{
					printf("Error in sending UDP message client -> remove client\n");
					remove_cli_node(&(*p_cli));
					send_msg_to_client(&(*p_cli), udpsockfd); // for this client starts this function on the beginning
					return;
				}
				remove_clients_own_msg_node(&(*p_cli)->p_msg); // poistaa viestin clientilta

			}
			// TCP viesti
			else if ((*p_msg)->protocol ==TCP)
			{
				if (sendpart((*p_cli)->tcpsockfd, (*p_msg)->msg, &((*p_cli)->p_msg->bytes_sended), &((*p_cli)->p_msg->bytes_left)) < 0)
				{
					printf("Error in sending message chat to client -> remove client.\n");
					remove_cli_node(&(*p_cli));
					send_msg_to_client(&(*p_cli), udpsockfd); // jatketaan seuraavasta
					return;
				}
				if ((*p_cli)->p_msg->bytes_left==0)
				{	// koko viesti luettu
					remove_clients_own_msg_node(&(*p_cli)->p_msg); // poistaa viestin clientilta
				}
			}
		}

		// jos rekisteröimättömälle asiakkaalle ei jäänyt yhtään viestiä, asiakas poistetaan (ei multicast)
		if ((*p_cli)!=multicast && strlen((*p_cli)->nick)==0 && (*p_cli)->p_msg==NULL)
		{
			remove_cli_node(&(*p_cli));
			send_msg_to_client(&(*p_cli), udpsockfd);	// jatketaan seuraavasta asiakkaasta
		}
		else if((*p_cli)->next != NULL)		
		{
			send_msg_to_client(&(*p_cli)->next, udpsockfd); // move to next client
		}
	}
}

// close and remove client
void remove_cli_node(cli_node_pointer *p)
{
	cli_node_pointer p_help;

	if((*p))
	{	
		close((*p)->tcpsockfd);  // closing TCP socket
		// puretaan clientin viestijono
		while((*p)->p_msg != NULL)
		{
			remove_clients_own_msg_node(&(*p)->p_msg);
		}
		// liitetään edellinen ja seuraava toisiinsa ja vapautetaan muisti poistettavan solmun osalta
		p_help=(*p);
		(*p)=(*p)->next;
		free(p_help);
		p_help=NULL;
	}
}

// poistaa lähtevän viestin clientilta. pävittää myös viestin keskeneräisten clienttien laskurin ja poistaa viestin kokonaan, mikäli menee nollaan
void remove_clients_own_msg_node(clients_own_msg_node_pointer *p)
{
	msg_node_pointer *p_msg;
	clients_own_msg_node_pointer p_help;

	if((*p))
	{
		p_msg=(*p)->p_msg;  // osoite viestin osoittimeen
		if (--((*p_msg)->unfinished_clients) <1) // onko lähetetty jo muille clienteille? -> poistetaan koko viesti
		{
			remove_msg_node((*p)->p_msg);
			if ((*p)->next) // lisää viestejä
			{	// korjataan seuraavan pointteri osoittamaan seuraavaan solmuun
				(*p)->next->p_msg=(*p)->p_msg;
			}
		}
		// liitetään edellinen ja seuraava toisiinsa ja vapautetaan muisti poistettavan solmun osalta
		p_help=(*p);
		(*p)=(*p)->next;
		free(p_help);
		p_help=NULL;
	}
}

// poistaa lähtevän viestin kokonaan
void remove_msg_node(msg_node_pointer *p)
{
	msg_node_pointer p_help;

	if((*p))
	{	
		// liitetään edellinen ja seuraava toisiinsa ja vapautetaan muisti poistettavan solmun osalta
		p_help=(*p);
		(*p)=(*p)->next;
		free(p_help);
		p_help=NULL;
	}
}

// tarkistaa, onko vähintään yksi rekisteröitynyt käyttäjä
int check_registered_clients(cli_node_pointer *p)
{
	if((*p))
	{
		if (strlen((*p)->nick)>0)  // rekisteröitynyt käyttäjä?
			return 1;
		if((*p)->next != NULL)
		{
			return check_registered_clients(&(*p)->next); // move to next client
		}
		else
			return 0;
	}
}

// lisää kysymyksen linkitettyyn listaan
void add_qstn_node(qstn_node_pointer *p, char *question, uint8_t nmb_of_options, int correct_answ_opt, char *options)
{
	if(!(*p))
	{	// are we end of the list -> add here new node
		if((*p = (qstn_node_pointer) malloc(sizeof(qstn_node_type))) == (qstn_node_pointer)NULL)
		{
			perror("malloc"); //exit(1);
		}
		(*p)->next = NULL;

		// asetaan jäsenien tiedot
		bzero((*p)->question, MAXLINE);
		strncpy((*p)->question, question, MAXLINE);
		(*p)->nmb_of_options=nmb_of_options;
		(*p)->correct_answ_opt=correct_answ_opt;
		bzero((*p)->options, MAXLINE);
		memcpy((*p)->options, options, MAXLINE);
	}	
	else
	{	// go to next node
		add_qstn_node(&(*p)->next, question, nmb_of_options, correct_answ_opt, options);
	}
}

// poistaa kysymyksen
void remove_qstn_node(qstn_node_pointer *p)
{
	qstn_node_pointer p_help;

	if((*p))
	{	
		// liitetään edellinen ja seuraava toisiinsa ja vapautetaan muisti poistettavan solmun osalta
		p_help=(*p);
		(*p)=(*p)->next;
		free(p_help);
		p_help=NULL;
	}
}


// palauttaa valitun kysymyksen osoitteen (tai NULL jos jotain menee pieleen)
qstn_node_pointer get_qstn_node(qstn_node_pointer *p, int counter, int rnd_qstn_nbm)
{
	if((*p))
	{
		if (counter==rnd_qstn_nbm)
		{	
			// etsittävä kysymys löytyi -> palautetaan osoite
			return (*p);
		}
		else if((*p)->next != NULL) // ei tätä kysymystä -> koitetaan seuraavaa
		{
			counter++;
			return get_qstn_node(&(*p)->next, counter, rnd_qstn_nbm); // move to next question
		}
		else
		{
			printf("Question not found for choice: %i.\n", rnd_qstn_nbm);
			return NULL;
		}
	}
}

// tarkistaa vastauksen, pisteet ja ilmoittaa rekisteröidyille asiakkaille oikeasta/väärästä vastauksesta
// tarkistaa myös että ip ja portti täsmää user_id:n kanssa. asiakkaalle lähetetään kuittausviesti riippuen tilanteesta
void check_answer(cli_node_pointer *p, uint32_t user_id, uint16_t question_id, uint8_t answer_choice, struct sockaddr_in p_client_addr, socklen_t p_cli_addr_len)
{
	if(!(*p))
	{	
		// invalid user id -> virheviesti asiakkaalle
		add_msg_node(&messages, &(*p), ERROR_INVALID_USER_ID, ONLY_TO_ONE);
	}	
	else
	{
		// tarkistetaan user_id:n perusteella oikea pelaaja
		if ((*p)->id==user_id)
		{	// tehdään tarkistuksia että tiedot täsmäävät
			// 1. ip ja portti
			if (p_cli_addr_len!=(*p)->cli_addr_len || memcmp((struct sockaddr *)&p_client_addr, (struct sockaddr*) &(*p)->client_addr, p_cli_addr_len) != 0)
			{
				printf("Answerred client have wrong ip address and/or port number.\n");
				return;	// just ignoring
			}
			// seuraavista virhetilanteista lähetetään ilmoitus asiakkaalle
			// 2. invalid message id
			else if (current_question_id != question_id)
			{
				add_msg_node(&messages, &(*p), ERROR_INVALID_MESSAGE_ID, ONLY_TO_ONE);
			}
			// 3. answer out of range
			else if (answer_choice<1 || answer_choice > current_question->nmb_of_options)
			{
				add_msg_node(&messages, &(*p), ERROR_ANSWER_OUT_OF_RANGE, ONLY_TO_ONE);
			}
			// 4. question already answered
			else if ((*p)->answer>0)
			{
				add_msg_node(&messages, &(*p), ERROR_QUESTION_ALREADY_ANSWERED, ONLY_TO_ONE);
			}
			else
			{	// vastausviesti läpäisi testit -> talletetaan vastaus, pisteet, lähetetään kuittaus asiakkaalle ja ilmoitus rekisteröidyille asiakkaille
				(*p)->answer=answer_choice;
				add_msg_node(&messages, &(*p), OK, ONLY_TO_ONE);
				bzero(sending_msg_buf, MAXLINE);
				strncpy(sending_msg_buf, (*p)->nick, strlen((*p)->nick));
				if (answer_choice != current_question->correct_answ_opt)
				{	// väärä vastaus -> ilmoitus väärästä vastauksesta
					(*p)->new_points=0;
					strncpy(&sending_msg_buf[strlen((*p)->nick)], " answered wrong", 15);
					printf("%s answered wrong.\n", (*p)->nick);
				}
				else
				{	// oikeasta vastauksesta pisteet
					(*p)->new_points=points;
					sprintf(&sending_msg_buf[strlen((*p)->nick)], " got %u points", points);
					printf("%s got %u points.\n", (*p)->nick, points);
				}
				add_msg_node(&messages, &clients, INFO, TO_REGISTERED_USERS);
			}
		}
		else
		{	// go to next node
			check_answer(&(*p)->next, user_id, question_id, answer_choice, p_client_addr, p_cli_addr_len);
		}
	}
}

// päivittää kokonaispisteet (ja nollaa uudet sekä vastauksen) ja lähettää ne rekisteröidyille asiakkaille. tarkistaa myös mahdollisen voittajan
void calculate_points_and_send(cli_node_pointer *p, uint16_t reg_user_count, uint16_t buf_ind)
{
	uint16_t len=0;

	if(!(*p))
	{	
		// täydennetään vielä viestin tyyppi ja pelaajien lkm ja lisätään viestipaketti rekisteröidyille asiakkaille
		strncpy(&sending_msg_buf[0], "S", 1);
		*(uint16_t*)&sending_msg_buf[1] = htons(reg_user_count);
		add_msg_node(&messages, &clients, SCORES, TO_REGISTERED_USERS);
	}	
	else
	{
		if (strlen((*p)->nick) >0) // vai rekisteröidyille käyttäjille
		{
			if (reg_user_count==0)
			{
				bzero(sending_msg_buf, MAXLINE);  // alustetaan bufferin ennen ekan pelaajan tietojen lisäystä
				sending_len=3; // lasketaan viestin pituus. viestin alun pituus on 3
			}
			reg_user_count++;
			// päivitetään kokonaispisteet ja nollataan uudet
			(*p)->total_points += (*p)->new_points;
			(*p)->new_points=0;
			(*p)->answer=0;
			// tarkistetaan onko meillä voittaja: eniten pisteitä saanut 50 ylittäjä
			if (winner!=NULL && winner->total_points < (*p)->total_points || winner==NULL && (*p)->total_points>WINNER_POINTS)
				winner=(*p);
			// täydennetään pisteviestiä: nick ja pisteet
			len=strlen((*p)->nick);
			strncpy(&sending_msg_buf[buf_ind], (*p)->nick, len);
			buf_ind += (len+1);
			*(uint8_t*)&sending_msg_buf[buf_ind] = (*p)->total_points;
			buf_ind++;
			sending_len += (len+2);
		}

		// go to next node
		calculate_points_and_send(&(*p)->next, reg_user_count ,buf_ind);
	}
}

// nollaa kokonaispisteet
void set_points_to_zero(cli_node_pointer *p)
{
	uint16_t len=0;

	if((*p)) // jos solmu on olemassa
	{
		(*p)->total_points=0;
		// go to next node
		set_points_to_zero(&(*p)->next);
	}
}

// poistaa asiakkaan user_id:n perusteella
void remove_client_by_user_id(cli_node_pointer *p, uint32_t user_id)
{
	if((*p)) // jos solmu on olemassa
	{
		if ((*p)->id == user_id)
		{	// löytyi oikea asiakas
			printf("User: %s quit the game.\n", (*p)->nick);
			remove_cli_node(&(*p));
		}
		else
		{
			// go to next node
			remove_client_by_user_id(&(*p)->next, user_id);
		}
	}
}

// yhdistää uuden TCP yhteyden user_id:n perusteella oikeaan asiakkaaseen
void connect_tcp_conn_and_client_by_user_id(cli_node_pointer *p, int connfd, uint32_t user_id)
{
	if((*p)) // jos solmu on olemassa
	{
		if ((*p)->id == user_id)
		{	// löytyi oikea asiakas
			(*p)->tcpsockfd = connfd;
		}
		else
		{
			// go to next node
			connect_tcp_conn_and_client_by_user_id(&(*p)->next, connfd, user_id);
		}
	}
}

// set tcp sockfd of clients to fdset
void set_tcp_cli_fdset(cli_node_pointer *p, fd_set *p_fdset)
{
	if((*p))
	{
		if ((*p)->tcpsockfd>0)
			FD_SET((*p)->tcpsockfd, p_fdset);	// set bit
		if((*p)->next != NULL)
		{
			set_tcp_cli_fdset(&(*p)->next, p_fdset); // move to next client
		}
	}
}

// listen part of message of all tcp clients which are ready to send
void read_tcp_msg(cli_node_pointer *p, fd_set *p_fdset)
{
	uint16_t chat_msg_len;
	uint32_t user_id=0;
	clients_own_msg_node_pointer *p_msg=NULL; // clientin oma viestijono

	if((*p))
	{
		if((*p)->tcpsockfd>0 && is_fd_ready((*p)->tcpsockfd, p_fdset))
		{	// we are ready to receive from client
			if ((*p)->bytes_recv_ed==0)
			{	// new message
				(*p)->bytes_left_rec=MAXLINE-1;	// don't know yet size of message
				bzero((*p)->message_rec, MAXLINE);
			}
			if (recvpart((*p)->tcpsockfd, (*p)->message_rec, &(*p)->bytes_recv_ed, &(*p)->bytes_left_rec) <0)	// receive part of message
			{
				printf("Error in receiving message from tcp socket: %i\n", (*p)->tcpsockfd);
				remove_cli_node(&(*p));	// closing socket
			}
			if ((*p)->bytes_left_rec == 0)	// is the whole message readed
			{
				// send message to other clients
				if ((*p)->message_rec[0]=='C')  // viestin tyyppi oltava chat
				{
					// puretaan asiakkaalta tullut viesti
					unpack_chat_from_client_msg((*p)->message_rec, &chat_msg_len, &user_id, chat_msg);
					// pakataan viesti muille asiakkaille
					sending_len = chat_from_server_message_packing(sending_msg_buf, (*p)->nick, chat_msg);
					add_msg_node(&messages, &clients, CHAT, TO_REGISTERED_USERS);
					// viesti pitää poistaa viestin lähettäneeltä clientilta (on tarkoitus lähettää vain muille)
					p_msg=&(*p)->p_msg;
					if (!(*p_msg)->next)
					{	// vain yksi viesti
						remove_clients_own_msg_node(&(*p_msg));
					}
					else
					{	// mennään listan loppuun
						while ((*p_msg)->next)
							(*p_msg)=(*p_msg)->next;
						remove_clients_own_msg_node(&(*p_msg));
					}
				}
				(*p)->bytes_recv_ed=0;
			}
		}
		if((*p) && (*p)->next != NULL)	// move to other clients
		{
			read_tcp_msg(&(*p)->next, p_fdset);
		}
	}
}
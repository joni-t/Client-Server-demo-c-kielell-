/* Joni Taipale, 0342424, joni.taipale@lut.fi */

#define MAXADVERTS 10
#define TIME_OUT 3	// udp viestien kuittauksen time-out 3s

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
#include <sys/stat.h>
#include <fcntl.h>

// Yksi solmu sisältää viestin, joka voidaan lähettää vaikka kaikille clienteille
typedef struct node_serv {
	char ip[15+1];	// server ip
	uint16_t udpport;	// udp port number
	uint16_t tcpport;	// tcp port number
	struct node_serv *next;
} serv_node_type, *serv_node_pointer;

int gameclient(char *nick, int channel);	// game client starts here
int gameclient_in_game(char *nick); // funktio käsittelee peliä koskevat toiminnot (asiakas on rekisteröitynyt)

void handling_advert_msg(char* msg_ptr, struct sockaddr_in server_reply_addr, serv_node_pointer *p_serv); // funktio purkaa mainosviestin ja lisää palvelimen tiedot linkitettyyn listaan
void add_serv_node(serv_node_pointer *p, char* ip, uint16_t udpport, uint16_t tcpport, int *counter); // add new server/advert node
void remove_serv_node(serv_node_pointer *p); // poistaa serverin/mainoksen solmun
void list_of_games(serv_node_pointer *p, int choice); // listaa mainostetut pelit
// liittyy, rekisteröi, ottaa yhteyden valitulle pelipalvelimelle
// palauttaa nollan mikäli kaikki menee putkeen, tai -1 mikäli jotain menee pieleen
int join_in_game(serv_node_pointer *p, int counter, int game_choice, char *nick);
void handling_question_msg(char* msg_ptr, uint16_t dgram_size_recv, uint16_t *current_question_id); // funktio purkaa kysymysviestin, tulostaa sen ja tallettaa kysymyksen id:n
// funktio pakkaa vastausviestin, lähettää sen ja odottaa kuittauksen (tai time-out)
// palauttaa nollan mikäli kaikki menee putkeen, tai -1 mikäli jotain menee pieleen
int send_answer(uint16_t current_question_id, uint8_t answer_choice);
void unpack_answer_msg(char* msg_ptr, uint32_t *user_id, uint16_t *question_id, uint8_t *answer_choice); // funktio purkaa vastausviestin (answer)
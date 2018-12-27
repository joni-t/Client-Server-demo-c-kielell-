/* Joni Taipale, 0342424, joni.taipale@lut.fi */

#define MAXUSERS 20
#define ADVERT_TIME_INTERVAL 30-1
#define GAME_TIME 60-1	// kuinka monta sekuntia peli kest‰‰
#define WINNER_POINTS 50-1
// viesti kaikille, reksiterˆidyille vai vain yhdelle
#define TO_ALL 1
#define ONLY_TO_ONE 2
#define TO_REGISTERED_USERS 3

#include "message_handling.h"

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
#include <fcntl.h>

// linked list for message queue
// Yksi solmu sis‰lt‰‰ viestin, joka voidaan l‰hett‰‰ vaikka kaikille clienteille
typedef struct node_msg {
	char msg[MAXLINE];	// message buffer
	uint16_t  length;
	int protocol;		// TCP tai UDP
	int unfinished_clients; // laskuri, kuinka monella clientilla viestin l‰hetys on kesken
	struct node_msg *next;
} msg_node_type, *msg_node_pointer;

// Linkitetty lista, clientin oma viestijono. Sis‰lt‰‰ l‰hetyksen tilan (kuinka paljon l‰hetetty) ja pointterin varsinaiseen viestiin (yhteinen kaikille clienteille, joille viesti l‰hetet‰‰n
typedef struct node_clients_own_msg {
	msg_node_pointer *p_msg;    // osoite viestiosoittimeen: voidaan hallita viestiosoitinta suoraan asiakkaan kautta
	uint16_t bytes_sended;
	uint16_t bytes_left;
	struct node_clients_own_msg *next;
} clients_own_msg_node_type, *clients_own_msg_node_pointer;

// linked list for clients
typedef struct node_cli {
	uint32_t id;		// user id
	char nick[MAXLENGTHNICK+1];
	uint8_t answer;			// vastattu jo vai ei? ei vastausta: 0, 0< vastausvaihtoehto
	uint8_t total_points;			// kokonaispisteet
	uint8_t new_points;				// uudet pisteet
	int tcpsockfd;				// sockfd for tcp
	struct sockaddr_in client_addr;		// address structure for udp
	socklen_t cli_addr_len;
	char message_rec[MAXLINE];		// message buffer for receiving message
	uint16_t bytes_recv_ed;		// how many bytes received
	uint16_t bytes_left_rec;		// how many bytes left receiving
	clients_own_msg_node_pointer p_msg; // clientin oma viestijono
	struct node_cli *next;
} cli_node_type, *cli_node_pointer;

// linked list for competition questions
typedef struct node_qstn {
	char question[MAXLINE];
	uint8_t nmb_of_options;			// vastausvaihtoehtojen m‰‰r‰
	uint8_t correct_answ_opt;		// oikea vastausvaihtoehto
	char options[MAXLINE];		// vastausvaihtoehdot erotettuna '\0' merkill‰
	struct node_qstn *next;
} qstn_node_type, *qstn_node_pointer;

// functions of linked list of clients
void add_cli_node(cli_node_pointer *p, uint16_t msg_type, int registered_counter, uint32_t id, char *nick, struct sockaddr_in p_client_addr, socklen_t p_cli_addr_len); // lis‰‰ uuden asiakkaan. parametrina annetaan viestin tyyppi, kummassa yhteydess‰ asiakas lis‰t‰‰n (mainospyyntˆ tai rekisterˆityminen) ja t‰m‰n perusteella muodostetaan paluuviesti. tarkistaa myˆs ettei pelaajien maksimim‰‰r‰‰ ylitet‰
void add_msg_node(msg_node_pointer *p, cli_node_pointer *p_client, uint16_t msg_type, int whom); // lis‰‰ viestin joko kaikille tai vain yhdelle clientille
void add_msg_to_client(cli_node_pointer *p_cli, msg_node_pointer *p_msg, int whom); // lis‰‰ (linkitt‰‰) viestin clientille. parametrilla p‰‰tet‰‰n, linkitet‰‰nkˆ viesti kaikille clienteille vai vain yhdelle
void add_clients_own_msg_node(clients_own_msg_node_pointer *p, msg_node_pointer *p_msg); // lis‰‰ clientille solmun, joka sis‰lt‰‰ osoitteen varsinaiseen viestiin ja pit‰‰ sis‰ll‰‰n viestin l‰hetyksen tilan
// funktio l‰hett‰‰ vanhimman viestin (tai sen osan) clientille ja siirtyy seuraavaan clienttiin
// viesti poistetaan (clientilta), kun l‰hetys saatu loppuun
void send_msg_to_client(cli_node_pointer *p_cli, int udpsockfd);
void remove_cli_node(cli_node_pointer *p); // close and remove client
void remove_clients_own_msg_node(clients_own_msg_node_pointer *p); // poistaa l‰htev‰n viestin clientilta. p‰vitt‰‰ myˆs viestin keskener‰isten clienttien laskurin ja poistaa viestin kokonaan, mik‰li menee nollaan
void remove_msg_node(msg_node_pointer *p); // poistaa l‰htev‰n viestin kokonaan
int check_registered_clients(cli_node_pointer *p); // tarkistaa, onko v‰hint‰‰n yksi rekisterˆitynyt k‰ytt‰j‰
void add_qstn_node(qstn_node_pointer *p, char *question, uint8_t nmb_of_options, int correct_answ_opt, char *options); // lis‰‰ kysymyksen linkitettyyn listaan
void remove_qstn_node(qstn_node_pointer *p); // poistaa kysymyksen
qstn_node_pointer get_qstn_node(qstn_node_pointer *p, int counter, int rnd_qstn_nbm); // palauttaa valitun kysymyksen osoitteen (tai NULL jos jotain menee pieleen)
// tarkistaa vastauksen, pisteet ja ilmoittaa rekisterˆidyille asiakkaille oikeasta/v‰‰r‰st‰ vastauksesta
// tarkistaa myˆs ett‰ ip ja portti t‰sm‰‰ user_id:n kanssa. asiakkaalle l‰hetet‰‰n kuittausviesti riippuen tilanteesta
void check_answer(cli_node_pointer *p, uint32_t user_id, uint16_t question_id, uint8_t answer_choice, struct sockaddr_in p_client_addr, socklen_t p_cli_addr_len);
void calculate_points_and_send(cli_node_pointer *p, uint16_t reg_user_count, uint16_t buf_ind); // p‰ivitt‰‰ kokonaispisteet (ja nollaa uudet) ja l‰hett‰‰ ne rekisterˆidyille asiakkaille. tarkistaa myˆs mahdollisen voittajan
void set_points_to_zero(cli_node_pointer *p); // nollaa kokonaispisteet
void remove_client_by_user_id(cli_node_pointer *p, uint32_t user_id); // poistaa asiakkaan user_id:n perusteella
void connect_tcp_conn_and_client_by_user_id(cli_node_pointer *p, int connfd, uint32_t user_id); // yhdist‰‰ uuden TCP yhteyden user_id:n perusteella oikeaan asiakkaaseen
void set_tcp_cli_fdset(cli_node_pointer *p, fd_set *p_fdset); // set tcp sockfd of clients to fdset
void read_tcp_msg(cli_node_pointer *p, fd_set *p_fdset); // listen part of message of all tcp clients which are ready to send

int gameserver(uint16_t p_udpport, uint16_t p_tcpport, int channel);	// game server starts here
// function that handles traffic between client and server, also handles command by user
int gameserver_connected(int mcastsockfd, int udpsockfd, int listenfd, int addrlen);

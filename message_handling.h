/* Joni Taipale, 0342424, joni.taipale@lut.fi */

#ifndef __MESSAGEHANDLING_H
#define __MESSAGEHANDLING_H

#define MAXLINE 4096	// max length of message packet	
#define MAXLENGTHNICK 50
// protocols
#define UDP 1
#define TCP 2
// message types
#define UNKNOWN -1
#define ADVERT 1
#define INQUIRE 2
#define REGISTER 3
#define CONNECTED 4
#define QUESTION 5
#define ANSWER 6
#define OK 7
#define INFO 8
#define SCORES 9
#define WINNER 10
#define GOODBYE 11
#define CHAT 12
#define ERROR 13  // yleinen virheviestityyppi
// tarkat virheviestityypit
#define ERROR_INVALID_MESSAGE_ID 15
#define ERROR_ANSWER_OUT_OF_RANGE 16
#define ERROR_QUESTION_ALREADY_ANSWERED 17
#define ERROR_INVALID_USER_ID 18
#define ERROR_NICKNAME_ALREADY_IN_USE 19
#define ERROR_SERVER_IS_FULL 20
#define ERROR_UNKNOWN_ERROR_SITUATION 21
// error codes
#define INVALID_MESSAGE_ID 1
#define ANSWER_OUT_OF_RANGE 2
#define QUESTION_ALREADY_ANSWERED 3
#define INVALID_USER_ID 4
#define NICKNAME_ALREADY_IN_USE 5
#define SERVER_IS_FULL 6
#define UNKNOWN_ERROR_SITUATION 7

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

int Call_Select(fd_set *rset, fd_set *wset, int sec, int max_fd);	// Function waiting message and and return nmb of ready descriptors
int is_fd_ready(int fd, fd_set *set);					// check is the fd ready to IO
int read_std_in(char *msg_ptr, int max_len);	// function read standard input until message is not too long. fuction remove line change on end

// handling partial recv()s 
// function update bytes_recv_ed and bytes_left
// function read in first time actual length from message (first two bytes) and update bytes_left
int recvpart(int sockfd, char *buf, uint16_t *bytes_recv_ed, uint16_t *bytes_left);
// handling partial send()s
// function update bytes_sended and bytes_left
// function don't send not more than packet_size at the time
int sendpart(int sockfd, char *buf, uint16_t *bytes_sended, uint16_t *bytes_left);


uint16_t advert_message_packing(char *message_ptr, uint16_t udpport, uint16_t tcpport); // funktio pakkaa pelin mainosviestin
uint16_t inquiry_message_packing(char *message_ptr, uint16_t udpport); // funktio pakkaa pelin tiedusteluviestin
uint16_t check_message_type(char* msg_ptr, uint16_t len); // funktio palauttaa viestin tyypin
void unpack_advert_msg(char* msg_ptr, uint16_t *udpport, uint16_t *tcpport); // funktio purkaa mainosviestin
void unpack_inquire_msg(char* msg_ptr, uint16_t *udpport); // funktio purkaa pelitiedusteluviestin
uint16_t registering_message_packing(char *message_ptr, char *nick); // funktio pakkaa pelin rekisteröitymisviestin
void unpack_connected_msg(char* msg_ptr, uint32_t *user_id); // funktio purkaa rekisteröintikuittausviestin (connected)
void unpack_error_msg(char* msg_ptr); // funktio purkaa virheviestin ja tulostaa virheen tyypi ja lisäviestin
void unpack_register_msg(char* msg_ptr, char *nick); // funktio purkaa rekisteröintiviestin
uint16_t connected_message_packing(char *message_ptr, uint32_t user_id); // funktio pakkaa rekisteröinnin kuittausviestin (connected)
uint16_t error_message_packing(char *message_ptr, uint8_t error_msg_type); // funktio pakkaa virheviestin
uint16_t question_message_packing(char *message_ptr, uint16_t next_question_id, char *question, uint8_t nmb_of_options, char *options); // funktio pakkaa kysymysviestin (question)
void unpack_question_msg(char* msg_ptr, uint16_t dgram_size_recv, uint16_t *current_question_id, char *question, uint8_t *nmb_of_options, char *options); // funktio purkaa kysymysviestin
uint16_t answerring_message_packing(char *message_ptr, uint32_t user_id, uint16_t current_question_id, uint8_t answer_choice); // funktio pakkaa vastausviestin (answer)
void unpack_ok_msg(char* msg_ptr, uint32_t *user_id, uint16_t *question_id); // funktio purkaa vastauskuittausviestin (ok)
uint16_t ok_message_packing(char *message_ptr, uint32_t user_id, uint16_t question_id); // funktio pakkaa vastauksen kuittausviestin (ok)
uint16_t info_message_packing(char *message_ptr, char *info); // funktio pakkaa infoviestin (info)
char* unpack_info_msg(char* msg_ptr); // funktio "purkaa" infoviestin vain palauttamalla osoitteen ensimmäiseen tavuun (info)
void unpack_scores_msg(char* msg_ptr, char *own_nick); // funktio purkaa pisteet viestin (tulostaa pisteet)
uint16_t goodbye_message_packing(char *message_ptr, uint32_t user_id); // funktio pakkaa goobye viestin
void unpack_goodbye_msg(char* msg_ptr, uint32_t *user_id); // funktio purkaa poistumisviestin (goodbye)
uint16_t chat_from_client_message_packing(char *message_ptr, uint32_t user_id, char *msg); // funktio pakkaa chat viestin clientilta (chat message from client)
void unpack_chat_from_client_msg(char* msg_ptr, uint16_t *len, uint32_t *user_id, char *msg); // funktio purkaa chat viestin clientilta (chat message from client)
uint16_t chat_from_server_message_packing(char *message_ptr, char *nick, char *msg); // funktio pakkaa chat viestin serveriltä (chat message from server)
void unpack_chat_from_server_msg(char* msg_ptr); // funktio purkaa chat viestin serveriltä, tulostaa sen (chat message from server)

#endif

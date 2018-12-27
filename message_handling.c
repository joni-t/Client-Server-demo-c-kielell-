/* Joni Taipale, 0342424, joni.taipale@lut.fi */


#include "message_handling.h"


// handling partial recv()s 
// function update bytes_recv_ed and bytes_left
// function read in first time actual length from message (first two bytes) and update bytes_left
int recvpart(int sockfd, char *buf, uint16_t *bytes_recv_ed, uint16_t *bytes_left)
{
	int n=0;
	uint16_t length;

	if (*bytes_recv_ed<3)
	{	// read first three bytes (type and length)
		n = recv(sockfd, buf+(*bytes_recv_ed), 3-*bytes_recv_ed, 0);
        	if (n != -1)
		{
			*bytes_recv_ed += n;
			if (*bytes_recv_ed==3)
			{	// read length
				length = ntohs(*(uint16_t*)&buf[1]);
				*bytes_left = length - *bytes_recv_ed;
			}
		}
		else
		{
			return -1;
		}
	}
	// read other bytes
	else
	{
	        n = recv(sockfd, buf+(*bytes_recv_ed), *bytes_left, 0);
	        if (n != -1)
		{
       			*bytes_recv_ed += n;
        		*bytes_left -= n;
		}
	}
    	return n==-1?-1:0; // return -1 on failure, 0 on success
}


// handling partial send()s
// function update bytes_sended and bytes_left
// function don't send not more than packet_size at the time
int sendpart(int sockfd, char *buf, uint16_t *bytes_sended, uint16_t *bytes_left)
{
	int n=0;

	{
	        n = send(sockfd, buf+(*bytes_sended), *bytes_left, 0);
	        if (n != -1)
		{
			*bytes_sended += n;
		       	*bytes_left -= n;
		}
	}

    	return n==-1?-1:0; // return -1 on failure, 0 on success
}

// Function waiting message and which one is ready for receiving
int Call_Select(fd_set *rset, fd_set *wset, int sec, int max_fd)
{	
	struct timeval tv;
	int return_value, i;

	tv.tv_sec=sec;	// set time out
	tv.tv_usec = 0;

	if(tv.tv_sec<0)
		return_value = select(max_fd+1, rset, wset, NULL, NULL);	// wait forever
	else
		return_value = select(max_fd+1, rset, wset, NULL, &tv);	// use time out
	return return_value;
}

// check is the fd ready to IO
int is_fd_ready(int fd, fd_set *set)
{
	return FD_ISSET(fd, set);
}

// function read standard input until message is not too long. max size is max_len-1
// fuction remove line change on end
int read_std_in(char *msg_ptr, int max_len)
{
	int n, select_value;
	fd_set rset;

	do	// read until suitable size
	{
		bzero(msg_ptr, max_len);
		if ((n=read(0, msg_ptr, max_len-1)) < 0)
		{
			perror("Error in reading user input");
			return -1;
		}
		// check is input too long
		FD_ZERO(&rset); // set all bits to zero
		FD_SET(0, &rset); // set bit for STD_IN
		select_value = Call_Select(&rset, NULL, 0, 0+1);
		if(select_value>0)	// too long, inform and empty STD_IN
			printf("Your input is too long. Possible size is %i or less.\n", max_len-1);
		FD_ZERO(&rset); // set all bits to zero
		FD_SET(0, &rset); // set bit for STD_IN
		while(Call_Select(&rset, NULL, 0, 0+1) > 0)	// luetaan loput
			read(0, msg_ptr, max_len);
	} while(select_value > 0);
	if (select_value < 0)
	{
		printf("Error in reading user input.\n");
		return -1;
	}
	// remove line change
	if (msg_ptr[n]=='\0')
		msg_ptr[n]='\0';
	if (msg_ptr[n-1]=='\0')
		msg_ptr[n-1]='\0';
	return strlen(msg_ptr);
}

// funktio pakkaa pelin mainosviestin
uint16_t advert_message_packing(char *message_ptr, uint16_t udpport, uint16_t tcpport)
{
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "GAMEADV", 7);
	*(uint16_t*)&message_ptr[8] = htons(udpport);
	*(uint16_t*)&message_ptr[10] = htons(tcpport);
	return 12; // viestin pituus
}

// funktio pakkaa pelin tiedusteluviestin
uint16_t inquiry_message_packing(char *message_ptr, uint16_t udpport)
{
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "GAMEINQ", 7);
	*(uint16_t*)&message_ptr[8] = htons(udpport);
	return 10; // viestin pituus
}

// funktio palauttaa viestin tyypin
uint16_t check_message_type(char* msg_ptr, uint16_t len)
{
	if (len==12 && strncmp(msg_ptr, "GAMEADV", 7)==0)
		return ADVERT;
	else if (len==10 && strncmp(msg_ptr, "GAMEINQ", 7)==0)
		return INQUIRE;
	else if (len==5 && strncmp(msg_ptr, "C", 1)==0)
		return CONNECTED;
	else if (len>1 && strncmp(msg_ptr, "E", 1)==0)
		return ERROR;
	else if (len>1 && strncmp(msg_ptr, "R", 1)==0)
		return REGISTER;
	else if (len>10 && strncmp(msg_ptr, "Q", 1)==0)
		return QUESTION;
	else if (len==8 && strncmp(msg_ptr, "A", 1)==0)
		return ANSWER;
	else if (len==7 && strncmp(msg_ptr, "K", 1)==0)
		return OK;
	else if (len>1 && strncmp(msg_ptr, "I", 1)==0)
		return INFO;
	else if (len>5 && strncmp(msg_ptr, "S", 1)==0)
		return SCORES;
	else if (len>2 && strncmp(msg_ptr, "W", 1)==0)
		return WINNER;
	else if (len==5 && strncmp(msg_ptr, "G", 1)==0)
		return GOODBYE;
	else
	{
printf("Tuntematon: %s\n", msg_ptr);
		printf("Unknown message type.\n");
		return UNKNOWN;
	}
}

// funktio purkaa mainosviestin
void unpack_advert_msg(char* msg_ptr, uint16_t *udpport, uint16_t *tcpport)
{
	*udpport = ntohs(*(uint16_t*)&msg_ptr[8]);
	*tcpport = ntohs(*(uint16_t*)&msg_ptr[10]);
}

// funktio purkaa pelitiedusteluviestin
void unpack_inquire_msg(char* msg_ptr, uint16_t *udpport)
{
	*udpport = ntohs(*(uint16_t*)&msg_ptr[8]);
}

// funktio pakkaa pelin rekisteröitymisviestin
uint16_t registering_message_packing(char *message_ptr, char *nick)
{
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "R", 1);
	strncpy(&message_ptr[1], nick, strlen(nick));
	return strlen(message_ptr); // viestin pituus
}

// funktio purkaa rekisteröintikuittausviestin (connected)
void unpack_connected_msg(char* msg_ptr, uint32_t *user_id)
{
	*user_id = ntohl(*(uint32_t*)&msg_ptr[1]);
}

// funktio purkaa virheviestin ja tulostaa virheen tyypi ja lisäviestin
void unpack_error_msg(char* msg_ptr)
{
	uint8_t error_code;

	error_code = *(uint8_t*)&msg_ptr[1];
	// print error type and message
	printf("Error type: %u, message: %s\n", error_code, &msg_ptr[2]);
}

// funktio purkaa rekisteröintiviestin
void unpack_register_msg(char* msg_ptr, char *nick)
{
	bzero(nick, MAXLENGTHNICK+1);
	strncpy(nick, &msg_ptr[1], MAXLENGTHNICK);
}

// funktio pakkaa rekisteröinnin kuittausviestin (connected)
uint16_t connected_message_packing(char *message_ptr, uint32_t user_id)
{
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "C", 1);
	*(uint32_t*)&message_ptr[1] = htonl(user_id);
	return 5; // viestin pituus
}

// funktio pakkaa virheviestin
uint16_t error_message_packing(char *message_ptr, uint8_t error_msg_type)
{
	uint8_t error_code=0;

	// lasketaan error_code error_type:sta
	error_code=error_msg_type-14;

	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "E", 1);
	*(uint8_t*)&message_ptr[1] = error_code;
	// kopioidaan selkokielinen viesti perään
	if (error_code==INVALID_MESSAGE_ID)
	{
		strncpy(&message_ptr[2], "Invalid message id", 19);
	}
	else if (error_code==ANSWER_OUT_OF_RANGE)
	{
		strncpy(&message_ptr[2], "Answer out of range", 19);
	}
	else if (error_code==QUESTION_ALREADY_ANSWERED)
	{
		strncpy(&message_ptr[2], "Question already answered", 26);
	}
	else if (error_code==INVALID_USER_ID)
	{
		strncpy(&message_ptr[2], "Invalid user id", 15);
	}
	else if (error_code==NICKNAME_ALREADY_IN_USE)
	{
		strncpy(&message_ptr[2], "Nickname already in use", 23);
	}
	else if (error_code==SERVER_IS_FULL)
	{
		strncpy(&message_ptr[2], "Server is full", 14);
	}
	else if (error_code==UNKNOWN_ERROR_SITUATION)
	{
		strncpy(&message_ptr[2], "Unknown error situation", 23);
	}
	return strlen(&message_ptr[2])+2; // viestin pituus
}

// funktio pakkaa kysymysviestin (question)
uint16_t question_message_packing(char *message_ptr, uint16_t next_question_id, char *question, uint8_t nmb_of_options, char *options)
{
	uint16_t len=0;
	uint16_t total_len=0;
	int counter=0;
	int i=0;

	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "Q", 1);
	*(uint16_t*)&message_ptr[1] = htons(next_question_id);
	*(uint8_t*)&message_ptr[3] = nmb_of_options;
	len=strlen(question);
	strncpy(&message_ptr[4], question, len);
	total_len=5+len;
	// vastausvaihtoehdot
	for (counter=0; counter<nmb_of_options; counter++)
	{
		len=strlen(&options[i]);
		strncpy(&message_ptr[total_len], &options[i], len);
		total_len += len+1;
		i += len+1;
	}
	return total_len; // viestin pituus
}

// funktio purkaa kysymysviestin
void unpack_question_msg(char* msg_ptr, uint16_t dgram_size_recv, uint16_t *current_question_id, char *question, uint8_t *nmb_of_options, char *options)
{
	uint16_t len=0;

	*current_question_id = ntohs(*(uint16_t*)&msg_ptr[1]);
	*nmb_of_options = *(uint8_t*)&msg_ptr[3];
	
	strncpy(question, &msg_ptr[4], strlen(&msg_ptr[4]));
	len=strlen(&msg_ptr[4])+5;
	memcpy(options, &msg_ptr[len], dgram_size_recv-len);
}

// funktio pakkaa vastausviestin (answer)
uint16_t answerring_message_packing(char *message_ptr, uint32_t user_id, uint16_t current_question_id, uint8_t answer_choice)
{
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "A", 1);
	*(uint32_t*)&message_ptr[1] = htonl(user_id);
	*(uint16_t*)&message_ptr[5] = htons(current_question_id);
	*(uint8_t*)&message_ptr[7] = answer_choice;
	return 8; // viestin pituus
}

// funktio purkaa vastauskuittausviestin (ok)
void unpack_ok_msg(char* msg_ptr, uint32_t *user_id, uint16_t *question_id)
{
	*user_id = ntohl(*(uint32_t*)&msg_ptr[1]);
	*question_id = ntohs(*(uint16_t*)&msg_ptr[5]);
}

// funktio purkaa vastausviestin (answer)
void unpack_answer_msg(char* msg_ptr, uint32_t *user_id, uint16_t *question_id, uint8_t *answer_choice)
{
	*user_id = ntohl(*(uint32_t*)&msg_ptr[1]);
	*question_id = ntohs(*(uint16_t*)&msg_ptr[5]);
	*answer_choice = *(uint8_t*)&msg_ptr[7];
}

// funktio pakkaa vastauksen kuittausviestin (ok)
uint16_t ok_message_packing(char *message_ptr, uint32_t user_id, uint16_t question_id)
{
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "K", 1);
	*(uint32_t*)&message_ptr[1] = htonl(user_id);
	*(uint16_t*)&message_ptr[5] = htons(question_id);
	return 7; // viestin pituus
}

// funktio pakkaa infoviestin (info)
uint16_t info_message_packing(char *message_ptr, char *info)
{
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "I", 1);
	strncpy(&message_ptr[1], info, strlen(info));
	return strlen(info)+1; // viestin pituus
}

// funktio "purkaa" infoviestin vain palauttamalla osoitteen ensimmäiseen tavuun (info)
char* unpack_info_msg(char* msg_ptr)
{
	return &msg_ptr[1];
}

// funktio purkaa pisteet viestin (tulostaa pisteet)
void unpack_scores_msg(char* msg_ptr, char *own_nick)
{
	uint16_t len=0;
	uint16_t user_count=0;
	uint8_t points=0;
	uint8_t own_points=0;
	int counter=0;
	int i=3; // ekan pelaajan nick:n paikka viestissä

	user_count = ntohs(*(uint16_t*)&msg_ptr[1]);

	// käydään viestiä läpi ja tulostetaan muiden pelaajien pisteet
	// omat pisteet napataan viestistä, kun tulee kohdalle ja ne tulostetaan erikseen muiden pelaajien pisteiden perään
	printf("Current game ended and total points are now:\n");
	for (counter=0; counter<user_count; counter++)
	{
		len=strlen(&msg_ptr[i]);
		if (strncmp(&msg_ptr[i], own_nick, len) ==0)
		{	// luetaan omat pisteet talteen
			i += len+1;
			own_points = *(uint8_t*)&msg_ptr[i++];
		}
		else
		{	// tulostetaan muun pelaajan pisteet
			printf("Player %s get", &msg_ptr[i]);
			i += len+1;
			points = *(uint8_t*)&msg_ptr[i++];
			printf(" %u points.\n", points);
		}
	}
	// tulostetaan vielä omat pisteet
	printf("YOU have %u points.\n", own_points);
}

// funktio pakkaa poistumisviestin (goobye)
uint16_t goodbye_message_packing(char *message_ptr, uint32_t user_id)
{
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "G", 1);
	*(uint32_t*)&message_ptr[1] = htonl(user_id);
	return 5; // viestin pituus
}

// funktio purkaa poistumisviestin (goodbye)
void unpack_goodbye_msg(char* msg_ptr, uint32_t *user_id)
{
	*user_id = ntohl(*(uint32_t*)&msg_ptr[1]);
}

// funktio pakkaa chat viestin clientilta (chat message from client)
uint16_t chat_from_client_message_packing(char *message_ptr, uint32_t user_id, char *msg)
{
	uint16_t len;
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "C", 1);
	len=strlen(msg)+7;
	*(uint16_t*)&message_ptr[1] = htons(len);
	*(uint32_t*)&message_ptr[3] = htonl(user_id);
	strncpy(&message_ptr[7], msg, strlen(msg));
	return len; // viestin pituus
}

// funktio purkaa chat viestin clientilta (chat message from client)
void unpack_chat_from_client_msg(char* msg_ptr, uint16_t *len, uint32_t *user_id, char *msg)
{
	*len = ntohs(*(uint16_t*)&msg_ptr[1]);
	*user_id = ntohl(*(uint32_t*)&msg_ptr[3]);
	bzero(msg, MAXLINE);
	strncpy(msg, &msg_ptr[7], strlen(&msg_ptr[7]));
}

// funktio pakkaa chat viestin serveriltä (chat message from server)
uint16_t chat_from_server_message_packing(char *message_ptr, char *nick, char *msg)
{
	uint16_t total_len;
	uint16_t nick_len;
	uint16_t chat_msg_len;
	bzero(message_ptr, MAXLINE);
	strncpy(message_ptr, "C", 1);
	nick_len=strlen(nick);
	chat_msg_len=strlen(msg);
	total_len=7+nick_len+chat_msg_len;
	*(uint16_t*)&message_ptr[1] = htons(total_len);
	*(uint16_t*)&message_ptr[3] = htons(nick_len);
	strncpy(&message_ptr[5], nick, nick_len);
	*(uint16_t*)&message_ptr[5+nick_len] = htons(chat_msg_len);
	strncpy(&message_ptr[7+nick_len], msg, chat_msg_len);
	return total_len; // viestin pituus
}

// funktio purkaa chat viestin serveriltä, tulostaa sen (chat message from server)
void unpack_chat_from_server_msg(char* msg_ptr)
{
	uint16_t total_len;
	uint16_t nick_len;
	uint16_t chat_msg_len;
	char nick[MAXLENGTHNICK+1];
	char msg[MAXLINE];

	total_len = ntohs(*(uint16_t*)&msg_ptr[1]);
	nick_len = ntohs(*(uint16_t*)&msg_ptr[3]);
	bzero(nick, MAXLENGTHNICK+1);
	strncpy(nick, &msg_ptr[5], nick_len);
	chat_msg_len = ntohs(*(uint16_t*)&msg_ptr[5+nick_len]);
	bzero(msg, MAXLINE);
	strncpy(msg, &msg_ptr[7+nick_len], chat_msg_len);
	// tulostetaan
	printf("Message form %s: %s\n", nick, msg);
}
Tekijän tiedot: Joni Taipale, 0342424, joni.taipale@lut.fi

**********************
KÄÄNTÄMINEN
**********************
Käännetään komennolla 'make' tai 'make build'. 'make clean' poistaa komentotiedostot. 'make debug_s' käynnistää debuggerin (serveri) ja 'make debug_c' (asiakas).

**********************
OHJELMAN AJAMINNEN
**********************
asiakasohjelma: ./gameclient -n <nickname> -c <1|2|3>
palvelinohjelma: ./gameserver -ptcp <tcp port> -pudp <udp port> -c <1|2|3>
(Parametrit voidaan antaa missä järjestyksessä tahansa.)


Asiakasohjelman Komennot rekisteröimättömässä tilassa:
/channel <1|2|3> - Select channel (add membership to multicast group)
/inquire <1|2|3> - Send an inquiry to channel (multicast address)
/listgames - Show list of advertised games
/join <game id on list> - Join to game with given id (local list)
/quit - Quit application

Asiakasohjelman komennot, kun ollaan rekisteröidytty palvelimelle:
/leave - Leave from game and close connections
/answer <answer option id> - Answer to question
<message to be sent> - Send chat message

**********************
OHJELMAN SULKEMINEN
**********************
Asiakasohjelma:
/leave (rekisteröityneessä tilassa): poistuu palvelimelta (TCP-yhteys suljetaan). Jättää multicastin päälle (kuuntelee mainoksia).
/quit (rekisteröimättömässä tilassa): ohjelma suljetaan kokonaan (muisti siivotaan)

**********************
TIEDOSTOLISTAUS (lähdekoodit)
**********************
gameclient.c
gameclient.h
gameclient_main.c
gameclient_main.h
gameserver.c
gameserver.h
gameserver_main.c
gameserver_main.h
message_handling.c
message_handling.h
Questions_and_answers.dat

**********************
TOTEUTETUT OMINAISUUDET
**********************
Kaikki harjoitustyön kuvauksessa mainitut kohdat (wiki-sivulla on toteutettu). Ei mitään ylimääräistä. Laitan tähän jokaisen kuvauskappaleen ja perään, lyhyen kommentin (lyhyesti sanottuna tarkoittaa että jokainen mainittu on toteutettu).

Description
Task for the home exam is to build a prototype for virtual tv-competition.
Tv-chat is constructed from two parts:
A) Server, which multicasts (adverts) game information on different “channels”, “broadcasts” (unicast to every client) tv-competition questions to registered users (UDP), receives answers (UDP), keeps score and implements public view chat (TCP).
B) Client which receives multicast adverts and sends inquiries to selected multicast address (channels). Client can answer to competition questions with unicast UDP, and can send messages to public view with TCP.
-Toteuttaa kaikki mainitut.

GENERIC ( 3 points )
Client must detect if there is no server in advertised address.
Client must detect if some UDP command (e.g. registration) produces no response from server and react to this properly.
To change servers on the fly client must first quit from the current server and clean up all connections.
One server can support at least 5 users at a time. Set the limit to 20 users max.
No duplicate nicknames are allowed on the server.
Server sends questions every minute and allows answers only to the current question.
A client can answer only once per question.
-Toteuttaa kaikki mainitut.

Error messages and types:
   1. Invalid message id
   2. Answer out of range
   3. Question already answered
   4. Invalid user id
   5. Nickname already in use
   6. Server is full
   7. Unknown error situation
-Virheviestejä lähetetään asianmukaisissa tilanteissa. Viimeistä virhetyyppiä (7) lukuunottamatta palvelimelta lähtee kaikki muut tyypit, kun tarvitaan. (Ei ole löydetty tilannetta, jossa tarvitsisi lähettää tyyppiä 7 olevaa virheviestiä.

MULTICAST ( 9 points )
Server adverts game in different channels (multicast addresses):
    *
      Channel 1: address = 224.0.0.241, port = 11111
    *
      Channel 2: address = 224.0.0.242, port = 11112
    *
      Channel 3: address = 224.0.0.243, port = 11113
Advert is sent every 30 seconds, advert contains ASCII string GAMEADV followed by UDP and TCP port numbers open on server. The advert message is sent to multicast address (to selected channel).
Advert:
								UDP port 	TCP port
G	A	M	E	A	D	V	\0	16 bit int 	16 bit int
-Juurikin näin se on toteutettu.

Client inquiries for a game from a single channel (multicast address). Inquiry is sent to multicast address at startup and user can also request the client to send an inquiry to server from command line if client is not already in a game. Client sends an UDP port number with the ASCII string GAMEINQ which server uses for sending replies back to client (unicast), the reply to inquiry is the same message as in server advertisement.
Inquiry:								UDP port
G	A	M	E	I	N	Q	\0	16 bit int
Reply:								UDP port 	TCP port
G	A	M	E	A	D	V	\0	16 bit int 	16 bit int 
-Näin se on tehty.

Client must be able to save at least 10 game adverts in a list (no duplicates!) which can be shown to user when requested. User can select one of these with local list id and client then can try to register the user on that particular server. When client is connected the advertisements sent by servers should be updated to the list. If the list is full remove the oldest entry and replace it with new one.
Do not allow the multicast messages to propagate from the subnet.
-Muuten kyllä, mutta viimeisestä lauseesta en ole ihan varma. (En ole säätänyt TTL tai mikä se on).

UDP ( 9 points )
Client registers to server by sending a message with 1 byte 'R' (Register) followed by selected nickname.
Register:
Type 									
R	N	i	c	k	n	a	m	e	\0
Server replies to this message with message containing 1 byte 'C' (Connected) when connection is accepted followed by a user id (32 bit unsigned integer) selected by server (random or sequential).
Connected:
Type 	User id
C	32 bit int
If connection is rejected server replies with message containing 1 byte 'E' (Error) followed by error message type and reason as ASCII string. If error is related to nickname user has to be prompted for another nickname.
Error:
Type 	Error type 							
E	8 bit int 	M	e	s	s	a	g	e
-Näin on tehty ja asianmukaiset virheviestit lähtevät myös.

Server keeps the competition always on-line if there is at least one registered user. Server sends a question every minute to all registered users. Clients can then reply to these questions by selecting an appropriate option. Question can contain various amount of options, only one of them is the correct one.
Question message starts with 1 byte 'Q' (Question), followed by the question id as 16 bit integer, followed by amount of options in question (8 bit integer). Further data in message is the question and options: first the question as ASCII string and following ASCII strings are the answering options separated by NULL's - up to the amount of options defined in the 3rd protocol packet field.
Question:
Type 	Question id 	Option count 	Question 		Option1 			OptionN 	
Q	16 bit int 	8 bit int 	N bytes 	\0	N bytes 	\0	… 	N bytes 	\0
Answer starts with 1 byte 'A' (Answer) followed by the user id (32 bit integer) sent by server when connection was established, followed by the message identifier and the answer choice (ranging from 1 to 255).
-Näin juuri.

Answer starts with 1 byte 'A' (Answer) followed by the user id (32 bit integer) sent by server when connection was established, followed by the message identifier and the answer choice (ranging from 1 to 255).
Answer:
Type 	User id 	Question id 	Answer choice
A	32 bit int 	16 bit int 	8 bit int
Server must acknowledge the answer sent by client with message starting with 'K' (oK) and followed by the user id (32 bit integer) and question id (16 bit integer) which was answered. If client doesn't receive acknowledgement answer must be retransmitted.
oK:
Type 	User id 	Question id
K	32 bit int 	16 bit int
If user selected an option that was out of range server sends a error message using UDP to that particular client, message starts with 1 byte 'E' (Error) and is followed by error message type (8 bit integer) and error description as ASCII string. Server must verify that the address (and port) is the same for the given user id, if not error message is sent.
Error:
Type 	Error type 							
E	8 bit int 	M	e	s	s	a	g	e
Server has a database of questions in a text (or binary) file (no SQL database is required nor recommended). The file contains question and answering options which server loads when starting.
Server sends notifications about correct and wrong answers to all registered clients with message containing 1 byte 'I' (Info) followed by the information message.
Info:
Type 												
I	I	n	f	o		M	e	s	s	a	g	e
-Näin on tehty.

Server calculates points for each client giving 10 point max. for each correct answer. Every 20 seconds reduces 2 points from the maximum. When a player gets 50 points the competition is reset (only points, not the connections), server broadcasts a message about the winner.
Server sends the scores to all registered clients before the new question is sent. The message starts with 1 byte 'S' (Scores) and is followed by 16 bit integer specifying the amount of users in game which is followed by the nickname of the user, NULL character and points as 8 bit integer. From this packet client can get own points by checking the nickname (server should not allow duplicate nicknames).
Scores:
Type 	User count 							Points 								Points
S	16 bit int 	N	i	c	k	1	\0	8 bit int 	…	N	i	c	k	9	\0	8 bit int 
-SCORES viestissä ilmoitetaan pelaajien pisteiden päivitetty kokonaistilanne. Clientit tulostaa muiden pelaajien pisteet listaan ja lopuksi omat pisteet.

When some user reaches the score limit server broadcasts a message to all participants. Message starts with 1 byte 'W' (Winner) and is followed by the winners nickname.
Winner:
Type 						
W	N	i	c	k	1	\0
-Juurikin näin.

Client can quit the game at any time by sending a quit message which starts with 1 byte 'G' (Goodbye) followed by user id (32 bit integer). Server should cleanup all data related to the client who has left.
Goodbye:
Type 	User id
G	32 bit int
-On tehty.

TCP ( 9 points )
TCP connection between client and server can be established after the server has accepted registration over UDP (server sent 'Connected' message).
Client should send a chat message immediately after the TCP connection is established so server can 'connect' a TCP connection to user id. [updated for clarification] 
-Tällä tavalla sen tein.

Implement sending of chat messages from client to server using TCP connections. Server relays these messages to other registered clients. Client sends a chat message containing 1 byte message identification 'C' followed by the length of the chat message (16 bit integer) and user id given by server at registration phase (32 bit integer) and finally the message as ASCII string.
Chat (client ? server): [updated for clarification]
Type 	Chat msg length 	User id 	Message
C	16 bit int 	32 bit int 	N bytes
Server sends the message to all users except the sender with same identifier byte 'C' followed by the total message length (16 bit integer), followed by the nickname length (16 bit integer) and the nickname as ASCII string and the message in same format as in the chat message sent by client (length as 16 bit integer and message as ASCII string).
Chat (server ? clients): [updated for clarification]
Type 	Total length 	Nick length 	Nickname 	Chat msg length 	Chat msg
C	16 bit int 	16 bit int 	N bytes 	16 bit int 	N bytes
If client is quitting (sent 'Goodbye' packet over UDP) the TCP connection between client and server should be cleaned. 
-Tätä tapaa noudatin.

Hints
Different structures for multicast and UDP unicast.
Make sure that TCP messages are fully received.
Note that if question data exists in a binary file a tool must be made to allow inputting questions! Therefore it might be more convenient to use a text file for questions.
Use unsigned integers in protocol messages.
Remember to use network order properly with integers!
-Kyllä ja käytin tekstitiedostoa kysymyksien "tietokantana". Ja sen muoto on tällainen (rivillä on vastausvaihtoehtojen lkm|oikea vastausvaihtoehto|kysymys|vastausvaihtoehdot):
|3|2|Mina vuonna Estonia upposi?|1993|1994|1995|
|3|1|Mina vuonna Suomi voitti jääkiekon MM-kultaa?|1995|1996|1997|
|3|3|Mina vuonna Suomi voitti ensimmaisen arvokisamitalinsa?|1985|1987|1988|
|2|2|Onko Suomen jalkapallomaajoukkue paassyt koskaan arvoturnaukseen?|Kylla|Ei|
|3|2|Mika on Suomen suurin jarvi?|Paijanne|Saimaa|Inarinjarvi|
|2|1|Kuinka monta rallin maailmanmestaruutta Tommi Makisella on?|4|6|
|4|3|Mina vuonna Neuvostoliitto hajosi?|1987|1989|1991|1992|
|3|3|Mina vuonna suomi liittyi EU:n jaseneksi?|1993|1994|1995|
|2|1|Liittyiko Suomi Euroon heti alusta sen kayttoonotosta|Kylla|Ei|
|3|2|Mina vuonna Berliinin muuri murtui?|1987|1989|1991|

*****************
PUUTTEITA
******************
-En ole havainnut yhtään puutteita.
******************

******************
LÄHTEET
******************
Kurssikirja: UNIX Network Programming (lähinnä TCP:tä ja UDP:tä koskevat kappaleet)
kurssin wiki-sivustolla olevat linkit oppaisiin (lähinnä multicast-opas)
tcp_viestien lähettämiseen otin mallia aika suoraan wiki-sivustolla olevan linkin kautta (pieniä muutoksia toki), mutta koodasin siitä samankaltaisen version viestien vastaanottoa varteen. (saattoi olla tämä linkki: Beej's Guide to Network Programming)
Eipä juuri oikeastaan muita lähteitä.
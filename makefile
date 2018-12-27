KAANTAJA=gcc
DEBUGGERI=ddd

TIEDOSTOT_SERVERI=gameserver_main.c gameserver.c message_handling.c connection_handling.c
OHJELMA_SERVERI=gameserver

TIEDOSTOT_ASIAKAS=gameclient_main.c gameclient.c message_handling.c connection_handling.c
OHJELMA_ASIAKAS=gameclient


all: build

build:
	$(KAANTAJA) $(TIEDOSTOT_SERVERI) -g -o $(OHJELMA_SERVERI)
	$(KAANTAJA) $(TIEDOSTOT_ASIAKAS) -g -o $(OHJELMA_ASIAKAS)

debug_s:
	$(KAANTAJA) $(TIEDOSTOT_SERVERI) -g -o $(OHJELMA_SERVERI)
	$(DEBUGGERI) $(OHJELMA_SERVERI)

debug_c:
	$(KAANTAJA) $(TIEDOSTOT_ASIAKAS) -g -o $(OHJELMA_ASIAKAS)
	$(DEBUGGERI) $(OHJELMA_ASIAKAS)

clean:
	rm $(OHJELMA_SERVERI)
	rm $(OHJELMA_ASIAKAS)

sanitize:
	rm *~
all: Client/WTFclient.c Server/WTFserver.c Server/serverFunctions.o
	gcc Client/WTFclient.c -o Client/WTF -lcrypto -lssl
	gcc -o Server/WTFserver Server/WTFserver.c Server/serverFunctions.o -lpthread -lcrypto -lssl

serverFunctions.o: Server/serverFunctions.c
	gcc -c Server/serverFunctions.c

clean:
	rm Server/serverFunctions.o Server/WTFserver Client/WTF

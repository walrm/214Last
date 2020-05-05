all: Server/WTFserver.c Server/serverFunctions.o
	gcc -o Server/WTFserver Server/WTFserver.c Server/serverFunctions.o -lpthread -g -lcrypto -lssl; gcc -g Client/WTFclient.c -o Client/WTF -lcrypto -lssl

serverFunction.o: Server/serverFunctions.c
	gcc -c Server/serverFunctions.c

clean:
	rm Server/serverFunctions.o Server/WTFserver Client/WTF

all: WTFserver.c serverFunctions.o
	gcc -o WTFserver WTFserver.c serverFunctions.o -lpthread -g -lcrypto -lssl

serverFunctions.o: serverFunctions.c
	gcc -c serverFunctions.c

clean:
	rm serverFunctions.o WTFserver
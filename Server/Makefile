all: WTFserver.c serverFunctions.o
	gcc -o Server WTFserver.c serverFunctions.o -lpthread

serverFunctions.o: serverFunctions.c
	gcc -c serverFunctions.c

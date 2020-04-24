all: WTFserver.c serverFunctions.o
	gcc -o Server WTFserver.c -lpthread serverFunctions.o

serverFunctions.o: serverFunctions.c
	gcc -c serverFunctions.c

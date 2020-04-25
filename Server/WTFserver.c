#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>

#include "serverFunctions.h"

//Method to print error and return -1 as error
void pError(char* err){
    printf("%s\n",err);
    exit(-1);
}

//Handles communication between the server and client socket
void* clientServerInteract(void* socket_arg){
    int socket = *(int *) socket_arg;
    int status;
    char buffer[256];
    
    bzero(buffer,256);
    status = read(socket, buffer, 255);
    if(status < 0){
        pError("ERROR reading from socket");
    }

    printf("Message From Client: %s\n", buffer);
    status = write(socket, "Message Received", 16);

    if(status < 0){
        pError("ERROR writing to socket");
    }

    if(strlen(buffer)>5)
    //TESTING CREATE FUNCTION
    char* projectName = malloc(strlen(buffer)-6);
    memcpy(projectName, &buffer[7], strlen(buffer)-6);
    projectName[strlen(projectName)]='\0'; //Grabbing project name from the client side
    printf("Project Name: %s\n", projectName);
    create(projectName, socket);
}

int main(int argc, char* argv[]){
    int socketfd, port, pid, clilen, newsocketfd;
    char buffer[256];
    struct sockaddr_in serv_addr; //address info for server client
    struct sockaddr_in cli_addr; //address info for client struct
    
    //Open socket for AF_INET (IPV4 Protocol) and SOCK_STREAM (reliable two way connection)
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0)
        pError("ERROR opening socket");
    
    //Create socket struct and fill information
    bzero((char *) &serv_addr, sizeof(serv_addr));
    port = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    //Print hostname in order to connect
    char hostname[1024];
    gethostname(hostname,1024);
    printf("%s\n",hostname);
    
    //Binds sockfd to info specified in serv_addr struct
    if (bind(socketfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        pError("ERROR on bind");
    }
    
    if(listen(socketfd,5)<0){ //socket accepts connections and has maximum backlog of 5
        pError("ERROR on listen");
    }

    clilen = sizeof(cli_addr);
    pthread_t threadID;

    //Client trying to connect and forks for each new client connecting
    while(1){
        newsocketfd = accept(socketfd, (struct sockaddr *) &cli_addr, &clilen);
        if(newsocketfd<0){
            pError("ERROR on accept");
        }
        
        if(pthread_create(&threadID,NULL,clientServerInteract,(void*)&newsocketfd) < 0){
            pError("ERROR on creating thread");
        }
    }

    /*TODO: CATCH EXIT SIGNAL AND DO ALL THESE CLOSES 
    if(pid == 0){
        close(socketfd);
        clientServerInteract(newsocketfd);
        exit(0);
    }else close(newsocketfd);
    */
    return 0;
}


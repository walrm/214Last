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
#include <signal.h>

#include "serverFunctions.h"


//Method to print error and return -1 as error
void pError(char* err){
    printf("%s\n",err);
    exit(-1);
}

void stopServer(int sigNum){
    printf("Closing Server Connection...\n");
    exit(1);
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
    status = write(socket, "Established Connection", 22);

    if(status < 0){
        pError("ERROR writing to socket");
    }

    bzero(buffer,256);
    status = read(socket, buffer, 255);

    //very inefficent way of locking and unlocking- should do for each project instead of whole repository
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    int command = (int)buffer[0] - 48;
    printf("command: %d\n",command);
    pthread_mutex_lock(&lock);
    if(command == 5){ //Create Command
        //Grabbing project name from the client side calling create function
        char* projectName = malloc(strlen(buffer)-1);
        memcpy(projectName, &buffer[1], strlen(buffer));
        projectName[strlen(projectName)]='\0'; 
        printf("Project Name: %s\n", projectName);
        create(projectName, socket);
        free(projectName);
    }else if(command == 6){ //Delete Command
        //Grabbing project name from the client side calling create function
        char* projectName = malloc(strlen(buffer)-1);
        memcpy(projectName, &buffer[1], strlen(buffer));
        projectName[strlen(projectName)]='\0'; 
        printf("Project Name: %s\n", projectName);
        // delete(projectName, socket);
        // free(projectName);

        //TODO: Delete function for Server side, find project name and rm using sys call
        DIR *cwd = opendir("./");
        struct dirent *currentINode = NULL;
        do{
            currentINode = readdir(cwd);
            if(currentINode!=NULL && currentINode->d_type == DT_DIR){
                if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                        continue;

                //Found project and delete using sys call
                if(strcmp(currentINode->d_name,projectName)==0){
                    char* systemCall = malloc(8+strlen(projectName));
                    strcat(systemCall,"rm -rf ");
                    strcat(systemCall,projectName);
                    printf("System command: %s\n",systemCall);
                    if(system(systemCall)<0)
                        pError("ERROR on destroy rm");
                    write(socket,"1",1); 
                    return;
                }

            }
        }while(currentINode!=NULL); 
        
        //project doesn't exist, return error
        write(socket,"0",1);
    }
    pthread_mutex_unlock(&lock);
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

    signal(SIGINT, stopServer);

    printf("Waiting for Client Connection...\n");
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


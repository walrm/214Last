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

int socketfd, newsocketfd;

//Method to print error and return -1 as error
void pError(char* err){
    printf("%s\n",err);
    exit(-1);
}

//signal handler for closing server with ctrl+c
void stopServer(int sigNum){
    printf("Closing Server Connection...\n");
    close(socketfd);
    close(newsocketfd);
    exit(0);
}

//TODO: commit command. Send manifest file over and have client update files and then receive commit file
//TODO: FREE 2 mallocs, close 2? things
void commit(char* projectName, int socket){
    DIR *cwd = opendir("./");
    if(cwd == NULL){
        possibleError(socket,"ERROR on opening directory");
        return;
    }

    struct dirent *currentINode = NULL;
    do{
        currentINode = readdir(cwd);
        if(currentINode!=NULL && currentINode->d_type == DT_DIR){
            if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                    continue;

            if(strcmp(currentINode->d_name,projectName)==0){
                write(socket,"1",1); 
                
                char* manifestPath = malloc(strlen(projectName)+13);
                bzero(manifestPath,sizeof(manifestPath));
                manifestPath[0] = '.';
                manifestPath[1] = '/';
                strcat(manifestPath,projectName);
                char m[10] = "/.Manifest";
                strcat(manifestPath, m);
                manifestPath[strlen(manifestPath)] = '\0';
                printf("MANIFEST PATH: %s\n", manifestPath);
                int manifestFD = open(manifestPath, O_RDONLY); 

                //Use stats to read total bytes of manifest file
                struct stat manStats;
                manifestFD = open(manifestPath, O_RDONLY);
                if(stat(manifestPath,&manStats)<0){
                    possibleError(socket,"ERROR reading manifest stats");
                    return;
                }

                int size = manStats.st_size; 
                int bytesRead = 0, bytesToRead = 0;
                char manBuffer[256];
                
                //Send size of manifest file to client
                sprintf(manBuffer,"%d", size);
                write(socket,manBuffer,strlen(manBuffer));
                write(socket," ",1);
                printf("manBuffer: %s\n",manBuffer);
                printf("manBuffer size: %d\n", strlen(manBuffer));

                //Send manifest bytes to client
                while(size > bytesRead){
                    bytesToRead = (size-bytesRead<256)? size-bytesRead : 255;
                    bzero(manBuffer,256);
                    bytesRead += read(manifestFD,manBuffer,bytesToRead);
                    printf("MANBUFFER:\n%s\n", manBuffer);
                    write(socket,manBuffer,bytesToRead);
                }

                //Receive status and then receive commit file
                int i = 0;
                char* status = malloc(2);
                
                read(socket,status,1); //status of commit from client

                //Read in commit file if success on client side
                char commit[20];
                sprintf(commit, ".Commit%d", i);
                int commitFD = open(commit, O_CREAT, 00777);
                while(commitFD == -1){
                    i++;
                    sprintf(commit, ".Commit%d", i);
                    commitFD = open(commit, O_CREAT, 00777);
                }
                
                commitFD = open(commit, O_RDWR);
                char s[2];
                char buffer[256];
                int totalBytes = 0, bytesRead = 0;
                do{
                    bzero(s,2);
                    read(socket,s,1);
                    if(buffer[0] != ' ')
                        strcat(buffer,s);
                }while(buffer[0] != ' ');

                printf("SIZE OF COMMIT FILE: %s\n", buffer);

                totalBytes = atoi(buffer);

                printf("TOTALBYTES: %d\n", totalBytes);

                while(bytesRead<totalBytes){
                    bytesToRead = (size-bytesRead<256)? size-bytesRead : 255;
                    bzero(buffer,256);
                    bytesRead += read(socket, buffer, bytesToRead);
                    printf("BUFFER READ: %s\n",buffer);
                    write(commitFD,buffer,bytesToRead);
                }

                

            }   

        }
    }while(currentINode!=NULL); //project doesn't exist

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

    //Grabs project name if it is not a rollback command
    char* projectName;
    if(command != 9){
        projectName = malloc(strlen(buffer)-1);
        memcpy(projectName, &buffer[1], strlen(buffer));
        projectName[strlen(projectName)]='\0'; 
        printf("Project Name: %s\n", projectName);
    }

    if(command == 0){ //Checkout 
        checkout(projectName, socket);
    }else if(command == 3){ //Commit
        commit(projectName,socket);
    }else if(command == 5){ //Create 
        create(projectName, socket);
    }else if(command == 6){ //Delete 
        delete(projectName, socket);
    }else if(command == 7){ //CurrentVersion
        currentVersion(projectName,socket);
    }
    free(projectName);
    pthread_mutex_unlock(&lock);
}

int main(int argc, char* argv[]){
    if(argc < 2)
        pError("Invalid number of arguments");
        
    int port, pid, clilen;
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

    //Client trying to connect and forks for each new client connecting
    printf("Waiting for Client Connection...\n");
    while(1){
        newsocketfd = accept(socketfd, (struct sockaddr *) &cli_addr, &clilen);
        if(newsocketfd<0){
            pError("ERROR on accept");
        }
        if(pthread_create(&threadID,NULL,clientServerInteract,(void*)&newsocketfd) < 0){
            pError("ERROR on creating thread");
        }
    }
    return 0;
}


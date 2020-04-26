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

int s1, s2;

//Method to print error and return -1 as error
void pError(char* err){
    printf("%s\n",err);
    exit(-1);
}

//signal handler for closing server with ctrl+c
void stopServer(int sigNum){
    printf("Closing Server Connection...\n");
    close(s1);
    close(s2);
    exit(0);
}

//Recurisve helper method to send
void sendFileInformation(char* path, int socket){
    DIR* cwd = opendir(path);
    if(cwd == NULL){
        possibleError(socket,"ERROR on opening directory");
        return;
    }

    struct dirent *currentINode = NULL;
    do{
        currentINode = readdir(cwd);
        if(currentINode==NULL) return;
        
        //INode is directory, send directory informaiton
        if(currentINode->d_type == DT_DIR){
            if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                    continue;
            char nextPath[PATH_MAX];
            strcpy(nextPath, path);
            strcat(strcat(nextPath, "/"), currentINode->d_name);
            write(socket,"3",1); //server letting client know it is sending a directory name
            
            printf("DIRECTORY PATH: %s\n", nextPath);

            char dBuffer[256];
            sprintf(dBuffer, "%d", strlen(nextPath));

            write(socket,dBuffer,strlen(nextPath));
            write(socket," ",1);
            write(socket,nextPath,strlen(currentINode->d_name));

            sendFileInformation(nextPath, socket);
        }else{
            write(socket, "4", 1); //server letting client know it is sending file

            char* filePath = malloc(strlen(path)+strlen(currentINode->d_name)+2);
            bzero(filePath,sizeof(filePath));
            strcat(filePath,path);
            strcat(filePath,"/");
            strcat(filePath,currentINode->d_name);
            filePath[strlen(filePath)] = '\0';
            printf("FILE PATH: %s\n", filePath);
            int fd = open(filePath, O_RDONLY); 
            
            struct stat fileStats;
            if(stat(filePath,&fileStats)<0){
                possibleError(socket,"ERROR reading file stats");
                return;
            }

            int size = fileStats.st_size; 
            int bytesRead = 0, bytesToRead = 0;
            char buffer[256];
            
            //Send size of manifest file to client
            sprintf(buffer,"%d", size);
            write(socket,buffer,strlen(buffer));
            write(socket," ",1);
            printf("buffer: %s\n",buffer); 
            printf("buffer size: %d\n", strlen(buffer));
            write(socket,filePath,strlen(filePath));
            write(socket, " ", 1);

            //Send manifest bytes to client
            while(size > bytesRead){
                bytesToRead = (size-bytesRead<256)? size-bytesRead : 255;
                bzero(buffer,256);
                bytesRead += read(fd,buffer,bytesToRead);
                printf("BUFFER:\n%s\n", buffer);
                write(socket,buffer,bytesToRead);
            }
        }
    }while(currentINode!=NULL);
    write(socket,"5",1); //Let's client know there are no more files to read
}

//Send project and all its files to the client
void checkout(char* projectName, int socket){
    //opens (repository) directory from root path and looks if project already exists
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
                write(socket,"1",1); //send to client that project was found
                
                //Find manifest path and open to read data
                char* projectPath = malloc(strlen(projectName)+3);
                bzero(projectPath,sizeof(projectPath));
                projectPath[0] = '.';
                projectPath[1] = '/';
                strcat(projectPath,projectName);
                projectPath[strlen(projectPath)] = '\0';
                printf("PROJECT PATH: %s\n", projectPath);

                sendFileInformation(projectPath,socket);

                free(projectPath);
                closedir(cwd);
                close(socket);
                return;
            }

        }
    }while(currentINode!=NULL); 
    write(socket,"0",1); //write to client project doesn't exist
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

    //Client trying to connect and forks for each new client connecting
    printf("Waiting for Client Connection...\n");
    while(1){
        newsocketfd = accept(socketfd, (struct sockaddr *) &cli_addr, &clilen);
        s1 = socketfd;
        s2 = newsocketfd;
        if(newsocketfd<0){
            pError("ERROR on accept");
        }
        if(pthread_create(&threadID,NULL,clientServerInteract,(void*)&newsocketfd) < 0){
            pError("ERROR on creating thread");
        }
    }
    return 0;
}


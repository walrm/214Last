#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

//Method to print error and return -1 as error
void pError(char* err){
    printf("%s\n",err);
    return -1;
}
void clientServerInteract(int socket){
    int status;
    char buffer[256];
    
    bzero(buffer,256);
    status = read(newsockfd, buffer, 255);
    if(status < 0)
        pError("ERROR reading from socket");

    prinft("Message: %s\n",buffer);

    status = write(newsockfd, "Message Received", 16);
    if(status < 0)
        pError("ERROR writing to socket");
}

int main(int argc, char* argv[]){
    int socketfd, port, pid;
    int newsockfd, clilen;
    char buffer[256];
    struct sockaddr_in serv_addr; //address info for server client
    struct sockaddr_in cli_addr; //address info for client struct
    
    //Open socket for AF_INET (IPV4 Protocol) and SOCK_STREAM (reliable two way connection)
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd < 0)
        pError("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));
    port = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        pError("ERROR on bind");
    
    listen(socketfd,5) //socket accepts connections and has maximum backlog of 5
    
    clilen = sizeof(cli_addr);
    //Client trying to connect
    while(1){
        newsocketfd = accept(socketfd, (struct sockaddr *) &cli_addr, &clilen);
        if(newsocketfd<0)
            pError("ERROR on accept");
        pid = fork();
        if(pid<0)
            pError("ERROR on fork");
        if(pid == 0){
            close(socketfd);
            clientServerInteract(newsocketfd);
            exit(0);
        }else close(newsocketfd);
    }
    return 0;
}


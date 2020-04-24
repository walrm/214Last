#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>

struct thread{
    int threadNum;
    int threadsocket;
};

//Method to print error and return -1 as error
void pError(char* err){
    printf("%s\n",err);
    exit(-1);
}

//Handles communication between the server and client socket
void* clientServerInteract(void* socket_arg){
    struct thread test = *(struct thread*)socket_arg;
    int socket = test.threadsocket;
    //int socket = *(int *) socket_arg;
    int status;
    char buffer[256];
    
    bzero(buffer,256);
    status = read(socket, buffer, 255);
    if(status < 0)
        pError("ERROR reading from socket");
    printf("Message From Client %d: %s\n",test.threadNum,buffer);
    status = write(socket, "Message Received", 16);
    if(status < 0)
        pError("ERROR writing to socket");


    //TODO: writing server side create function
    char* projectName;
    memcpy(projectName, &buffer[7], strlen(buffer)-6);
    projectName[strlen(projectName)]='\0'; //Grabbing project name from the client side
    printf("Project Name: %s\n", projectName);

    //opens (repository) directory from root path
    DIR *cwd = opendir("./");          
    struct dirent *currentINode = NULL;
    do{
        currentINode = readdir(cwd);
        if(currentINode!=NULL && currentINode->d_type == DT_DIR){
            if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                    continue;
            printf("%s\n",currentINode->d_name);
            if(strcmp(currentINode->d_name,projectName)==0)
                pError("ERROR. Project already exists in repository");
        }
    }while(currentINode!=NULL);

    //Project doesn't already exist, create project folder
    mkdir(projectName,0700); 
    char* path = malloc(strlen(projectName)+12);
    path[0] = '.';
    path[1] = '/';
    strcat(path,projectName);
    char manifest[10] = "/.Manifest";
    strcat(path,manifest);
    printf("PROJECT PATH to Manifest: %s\n", path);

    //Create .Manifest file
    int manifestFD = open(path, O_CREAT | O_RDWR, 00777);
    write(manifestFD,"0\n",1);   
    //cwd = opendir()
    
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
    if (bind(socketfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        pError("ERROR on bind");
    
    listen(socketfd,5); //socket accepts connections and has maximum backlog of 5
    clilen = sizeof(cli_addr);
    pthread_t threadID;
    int i = 1;

    //Client trying to connect and forks for each new client connecting
    while(1){
        newsocketfd = accept(socketfd, (struct sockaddr *) &cli_addr, &clilen);
        if(newsocketfd<0)
            pError("ERROR on accept");
            
        struct thread t;
        t.threadNum = i++;
        t.threadsocket = newsocketfd;
        
        if(pthread_create(&threadID,NULL,clientServerInteract,&t) < 0)
            pError("ERROR on creating thread");
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


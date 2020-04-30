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

#include <openssl/md5.h>

#include "serverFunctions.h"

int socketfd, newsocketfd;

typedef struct _node
{
    char *fileName;
    char *hash;
    char* liveHash;
    int code; //0:Nothing,1:Add,2:Modify,3:Delete
    int version;
    struct _node *next;
} Node;

typedef struct _manifest
{
    int manifestVersion;
    Node *files;
} Manifest;

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

/**Adds the given byte to the end of the string
 * by freeing and reallocating memory to the string
 * 
 * @param str: the string to add the byte to. Must have allocated memory
 * @param byte: the byte to add at the end of the string
 * @return the concatenated string
 */
char *addByteToString(char *str, char *byte)
{
    char *temp = calloc(strlen(str) + strlen(byte) + 1, 1);
    strcat(temp, str);
    strcat(temp, byte);
    temp[strlen(temp)] = '\0';
    free(str);
    str = calloc(strlen(temp) + 1, 1);
    strcpy(str, temp);
    free(temp);
    return str;
}

/*Gets the live hash of a file given the Node* for the file from the manifest
 * 
 */
char *computeLiveHash(Node *ptr, char *projectName)
{
    return "";
    // unsigned char result[MD5_DIGEST_LENGTH];
    // char *filePath = calloc(strlen(projectName) + strlen(ptr->fileName) + 2, 1);
    // strcat(filePath, projectName);
    // strcat(filePath, "/");
    // strcat(filePath, ptr->fileName);
    // int fd = open(filePath, O_RDWR, 00777);
    // int fileSize = getFileSize(fd);
    // char *file_buffer = calloc(fileSize + 1, 1);
    // read(fd, file_buffer, fileSize);
    // MD5(file_buffer, fileSize, result);
    // int i = 0;
    // char *str = calloc(33, 1);
    // for (i = 0; i < MD5_DIGEST_LENGTH; i++)
    // {
    //     sprintf(&str[i * 2], "%02x", (unsigned int)result[i]);
    // }
    // close(fd);
    // free(file_buffer);
    // free(filePath);
    // return str;
}

/**
 * Creates a manifest struct given the fileDescriptor to read from that starts with the first byte of the manifest file,
 * the total bytes to read
 * the name of the project of the manifest file
 * and whether the manifest file is on the client side or the server side
 * @param fd the file descriptor to read from
 * @param totalBytes the total bytes to read from the fileDescriptor
 * @param projectName the name of the project that the commit/manifest file is for
 * @param isClientSide 1 if the project is local, 0 otherwise
 * @param isManifestFile 1 if it is from a manifest file, 0 otherwise
 * @return the manifest file, with each file being held in a Node*
 */
Manifest *createManifestStruct(int fd, int totalBytes, char *projectName, int isLocal, int isManifestFile)
{
    printf("Creating Manifest Struct\n");
    Manifest *man = (Manifest *)malloc(sizeof(Manifest));
    int bytesRead = 0;
    char *s = calloc(2, 1);
    char *clientProjectVersionS = calloc(10, 1);
    s[0] = 'a';
    s[1] = '\0';
    if (isManifestFile)
    {
        while (s[0] != '\n')
        {
            bytesRead += read(fd, s, 1);
            if (s[0] != '\n')
            {
                strcat(clientProjectVersionS, s);
            }
        }
        man->manifestVersion = atoi(clientProjectVersionS);
    }
    else
    {
        man->manifestVersion = -1;
    }

    char *gettingTotalBytes = calloc(2, 1);
    gettingTotalBytes[1] = '\0';
    char *name = calloc(1, 1);
    int readingName = 1;
    int readingCode = 0;
    int readingCodeB1 = 0;
    int readingHash = 0;
    int readingVersion = 0;
    int boolean = 0;
    man->files = malloc(sizeof(Node));
    Node *ptr = man->files;
    if (bytesRead >= totalBytes)
    {
        free(ptr);
        ptr = NULL;
        man->files = NULL;
        boolean = 1;
        printf("here\n");
    }
    while (bytesRead < totalBytes)
    {
        //printf("BytesRead:%d\nBytesTotal:%d\n",bytesRead,totalBytes);
        if (gettingTotalBytes[0] == '\n')
        {
            ptr->next = (Node *)malloc(sizeof(Node));
            ptr = ptr->next;
        }
        bytesRead += read(fd, gettingTotalBytes, 1);
        if (readingName) //reading the name of the file
        {
            if (gettingTotalBytes[0] == ' ')
            {
                readingName = 0;
                readingCodeB1 = 1;
                ptr->fileName = malloc(strlen(name));
                strcpy(ptr->fileName, name);
                free(name);
                name = calloc(1, 1);
            }
            else
            {
                name = addByteToString(name, gettingTotalBytes);
            }
        }
        else if (readingCodeB1) //reading the first byte of code/version
        {
            name = addByteToString(name, gettingTotalBytes);
            if (gettingTotalBytes[0] == '$')
            {
                readingCode = 1;
                readingCodeB1 = 0;
            }
            else
            {
                ptr->code = 0;
                readingCodeB1 = 0;
                readingVersion = 1;
            }
        }
        else if (readingCode) //reading the code
        {
            if (gettingTotalBytes[0] == ' ')
            {
                if (strcmp(name, "$M") == 0)
                { //Modify
                    ptr->code = 2;
                }
                else if (strcmp(name, "$A") == 0)
                { //Add
                    ptr->code = 1;
                }
                else
                { //Remove
                    ptr->code = 3;
                }
                free(name);
                name = calloc(1, 1);
                readingVersion = 1;
                readingCode = 0;
            }
            else
            {
                name = addByteToString(name, gettingTotalBytes);
            }
        }
        else if (readingVersion) //reading the version
        {
            if (gettingTotalBytes[0] == ' ')
            {
                printf("Version:%s\n", name);
                ptr->version = atoi(name);
                free(name);
                name = calloc(1, 1);
                readingVersion = 0;
                readingHash = 1;
            }
            else
            {
                name = addByteToString(name, gettingTotalBytes);
            }
        }
        else if (readingHash) //reading the hash
        {
            if (gettingTotalBytes[0] == '\n')
            {
                readingName = 1;
                readingHash = 0;
                ptr->hash = calloc(strlen(name) + 1, 1);
                strcpy(ptr->hash, name);
                if (isLocal && ptr->code != 3)
                {
                    ptr->liveHash = computeLiveHash(ptr, projectName);
                }
                else
                {
                    ptr->liveHash = calloc(strlen(ptr->hash) + 1, 1);
                    strcpy(ptr->liveHash, ptr->hash);
                }
                if (strcmp(ptr->liveHash, ptr->hash) != 0)
                {
                    ptr->code = 2; //Modify
                }

                free(name);
                name = calloc(1, 1);
            }
            else
            {
                name = addByteToString(name, gettingTotalBytes);
            }
        }
    }
    if (!boolean)
    {
        free(ptr->next);
        ptr->next = NULL;
    }
    free(name);
    free(clientProjectVersionS);
    free(s);
    free(gettingTotalBytes);
    return man;
}

//TODO: commit command. Send manifest file over and have client update files and then receive commit file
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
                sprintf(manifestPath,"./%s/.Manifest",projectName);
                printf("MANIFEST PATH: %s\n", manifestPath);
                int manifestFD = open(manifestPath, O_RDONLY); 

                //Use stats to read total bytes of manifest file
                struct stat manStats;
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
                int st = atoi(status);
                free(status);
                if(st == 0){
                    free(manifestPath);
                    close(manifestFD);
                    return;
                }

                //Read in commit file if success on client side
                char *commit = malloc(strlen(projectName)+13);
                sprintf(commit, "./%s/.Commit%d", projectName,i);
                int commitFD = open(commit, O_CREAT, 00777);
                while(commitFD == -1){
                    i++;
                    sprintf(commit, "./%s/.Commit%d", projectName,i);
                    commitFD = open(commit, O_CREAT, 00777);
                }
                
                commitFD = open(commit, O_RDWR);
                char s[2];
                char buffer[256];
                int totalBytes = 0;
                bytesRead = 0;

                do{
                    bzero(s,2);
                    read(socket,s,1);
                    if(s[0] != ' ')
                        strcat(buffer,s);
                }while(s[0] != ' ');

                totalBytes = atoi(buffer);

                printf("TOTALBYTES: %d\n", totalBytes);

                //write out commit file from client
                while(bytesRead<totalBytes){
                    bytesToRead = (totalBytes-bytesRead<256)? totalBytes-bytesRead : 255;
                    bzero(buffer,256);
                    bytesRead += read(socket, buffer, bytesToRead);
                    printf("BUFFER READ: %s\n",buffer);
                    write(commitFD,buffer,bytesToRead);
                }

                write(socket,"1",1); //write to client - commit successfully stored in server side
                
                free(commit);
                free(manifestPath);
                close(commitFD);
                close(manifestFD);
                return;
            }   

        }
    }while(currentINode!=NULL); //project doesn't exist
    write(socket,"0", 1); //write to client - project does not exist
}

void expireCommits(char* path){
    DIR *cwd = opendir(path);
    if(cwd == NULL){
        possibleError(socket,"ERROR on opening directory");
        return;
    }

    struct dirent *currentINode = NULL;
    do{
        currentINode = readdir(cwd);
        if(currentINode!=NULL && currentINode->d_type != DT_DIR){
            if(strlen(currentINode->d_name)>7 && strncmp(currentINode->d_name,".Commit",7)==0){
                char* commitFile = malloc(strlen(path)+strlen(currentINode->d_name)+2);
                sprintf(commitFile,"%s/%s",path,currentINode->d_name);
                
                char* systemCall = malloc(strlen(commitFile)+4);
                sprintf(systemCall, "rm %s",commitFile);
                printf("REMOVE CALL: %s\n",systemCall);
                int status = system(systemCall);

                free(commitFile);
                free(systemCall);
            }
        }
    }while(currentINode!=NULL);
}

//Search for matching commit file in the project, 
int searchforCommit(char* path, int socket){
    DIR *cwd = opendir(path);
    if(cwd == NULL){
        possibleError(socket,"ERROR on opening directory");
        return;
    }

    struct dirent *currentINode = NULL;
    do{
        currentINode = readdir(cwd);
        if(currentINode!=NULL && currentINode->d_type != DT_DIR){
            if(strlen(currentINode->d_name)>7 && strncmp(currentINode->d_name,".Commit",7)==0 && strcmp(currentINode->d_name,".Commit00")!=0){
                printf("Found a Commit File\n");

                char *difference = malloc(36);
                sprintf(difference, "diff %s %s/.Commit > %s/.Commit00", currentINode->d_name,path,path);
                printf("SYSTEM CALL: %s\n", difference);
                int status = system(difference);
                
                char* checkFile = malloc(11 + strlen(path));
                bzero(checkFile,sizeof(checkFile));
                strcat(checkFile,path);
                strcat(checkFile,"/.Commit00");

                int clientFD = open(checkFile,O_RDONLY);
                status = read(clientFD,difference,1);
                if(status == 0){
                    printf("THIS IS THE FILE!\n");
                    write(socket,"1",1); //write to client - commit file found

                    /*
                    char s[2];
                    char buffer[256];
                    int totalBytes = 0, bytesRead = 0;
                    do{
                        bzero(s,2);
                        read(socket,s,1);
                        if(s[0] != ' ')
                            strcat(buffer,s);
                    }while(s[0] != ' ');

                    totalBytes = atoi(buffer);
                    printf("TOTALBYTES: %d\n", totalBytes);

                    char tarFile[16] = "archive.tar.gz";
                    int tarFD = open(tarFile, O_CREAT|O_RDWR, 00777);

                    int bytesToRead = 0;
                    //Accept files from client
                    while(bytesRead<totalBytes){
                        bytesToRead = (totalBytes-bytesRead<256)? totalBytes-bytesRead : 255;
                        bzero(buffer,256);
                        bytesRead += read(socket, buffer, bytesToRead);
                        printf("BUFFER READ: %s\n",buffer);
                        write(tarFD,buffer,bytesToRead);
                    }
                    
                    //Write all files the client sent to project
                    int untarStatus = system("tar -xzf archive.tar.gz"); //untar the file
                    */
                    expireCommits(path); //Expire all other commits
                    
                    //-------------------------------------------------------------------------------------------------

                    //update project directory's .manifest replacing information from .commit, increase projects version
                    char* commitFile = malloc(strlen(path)+9);
                    sprintf(commitFile,"%s/.Commit", path);
                    
                    struct stat commitStats;
                    if(stat(commitFile,&commitStats)<0){
                        possibleError(socket,"ERROR reading manifest stats");
                        return;
                    }

                    char* manifestFile = malloc(strlen(path)+11);
                    sprintf(manifestFile, "%s/.Manifest", path);
                    struct stat manStats;
                    if(stat(manifestFile,&manStats)<0){
                        possibleError(socket,"ERROR reading manifest stats");
                        return;
                    }

                    int commitFD = open(commitFile, O_RDWR);
                    int manifestFD = open(manifestFile, O_RDWR);
                    int manifestSize = manStats.st_size; 
                    int commitSize = commitStats.st_size; 
                    Manifest* commit = createManifestStruct(commitFD,commitSize,"",0,0);
                    Manifest* man = createManifestStruct(manifestFD,manifestSize,"",0,1);
                    
                    Node* commitptr = commit->files;
                    Node* manptr = man->files;
                    man->manifestVersion++;
                    
                    while(commitptr != NULL){
                        if(commitptr->code == 3){
                            //Remove file 
                            while(strcmp(manptr->fileName, commitptr->fileName)!=0)
                                manptr=manptr->next;
                            manptr->fileName = "";
                            char* systemCall = malloc(strlen(manptr->fileName)+4);
                            sprintf(systemCall, "rm %s", manptr->fileName);
                            int rmStatus = system(systemCall);
                            free(systemCall);
                        }else if(commitptr->code == 1){
                            commitptr->code = 0;
                            Node* temp = manptr->next;
                            manptr->next = commitptr;
                            manptr->next->next = temp;
                        }else{
                            //Update hash, code, version
                            while(strcmp(manptr->fileName, commitptr->fileName)!=0)
                                manptr=manptr->next;
                            manptr->hash = commitptr->hash;
                            manptr->code = 0;
                            manptr->version++;
                        }
                        commitptr = commitptr->next;
                        manptr = man->files;
                    }
                    
                    //Update manifest file through the manifest structure
                    manifestFD = open(manifestFile, O_RDWR);
                    char* manVersion = malloc(sizeof(man->manifestVersion)/4+1);
                    sprintf(manVersion, "%d\n", man->manifestVersion);
                    printf("new man version and length: %s,%d\n", manVersion, strlen(manVersion));
                    write(manifestFD, manVersion, strlen(manVersion));
                    
                    //Write out new manifest file
                    while(manptr != NULL){
                        if(strcmp(manptr->fileName,"")!=0){
                            write(manifestFD, manptr->fileName, strlen(manptr->fileName));
                            write(manifestFD," ",1);
                            char* fileVersion = malloc(sizeof(manptr->version)/4+1);
                            sprintf(fileVersion, "%d", manptr->version);
                            write(manifestFD, fileVersion, strlen(fileVersion));
                            write(manifestFD," ",1);
                            write(manifestFD, manptr->hash, strlen(manptr->hash));
                            write(manifestFD,"\n",1);
                        }
                    }

                }
            }
        }
    }while(currentINode != NULL);
    return 0;
}

//TODO: push command for server side
void push(char* projectName, int socket){
    //Look for project and then take in commit and then look for the same commit file in the project
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
                write(socket,"1", 1); //write to client project found
                
                char* commit = malloc(strlen(projectName)+11);
                sprintf(commit, "./%s/.Commit",projectName);
                int commitFD = open(commit, O_CREAT|O_RDWR, 00777);

                char s[2];
                char buffer[256];
                int totalBytes = 0, bytesRead = 0;
                do{
                    bzero(s,2);
                    read(socket,s,1);
                    if(s[0] != ' ')
                        strcat(buffer,s);
                }while(s[0] != ' ');

                totalBytes = atoi(buffer);
                printf("TOTALBYTES: %d\n", totalBytes);

                int bytesToRead = 0;
                //write out commit file from client
                while(bytesRead<totalBytes){
                    bytesToRead = (totalBytes-bytesRead<256)? totalBytes-bytesRead : 255;
                    bzero(buffer,256);
                    bytesRead += read(socket, buffer, bytesToRead);
                    printf("BUFFER READ: %s\n",buffer);
                    write(commitFD,buffer,bytesToRead);
                }
                
                //Grab project path and pass to helper method to find commit file
                char* path = malloc(strlen(projectName)+3);
                if(path == NULL){
                    possibleError(socket,"ERROR on malloc");
                    return;
                }

                sprintf(path,"./%s",projectName);
                searchforCommit(path,socket);

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
    
    searchforCommit("./kttest1",socket);
    // if(command == 0){ //Checkout 
    //     checkout(projectName, socket);
    // }else if(command == 3){ //Commit
    //     commit(projectName,socket);
    // }else if(command == 4){ //Push
    //     push(projectName, socket);
    // }else if(command == 5){ //Create 
    //     create(projectName, socket);
    // }else if(command == 6){ //Delete 
    //     delete(projectName, socket);
    // }else if(command == 7){ //CurrentVersion
    //     currentVersion(projectName,socket);
    // }
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


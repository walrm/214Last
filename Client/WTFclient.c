#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <sys/stat.h>
#include <math.h>
#include <dirent.h>
#include <errno.h>
/**
 * $C-File is changed
 * $D-File is deleted
 * $A-File is added
 * 0-Create
 * 
 */
int min(int a, int b)
{
    if (a > b)
    {
        return b;
    }
    return b;
}
/**
 * Looks for the .configure file. If it does not exist, prints an error.
 * If it does exist, it attempts to connect to the server 
 * If it fails, prints an error
 * If success, returns the file descriptor of the server
 * 
 * Return: On success: File Descriptor of the server. On fail: -1
 */
int checkConnection()
{
    if (access(".configure", F_OK) == -1)
    {
        printf("Error: .configure file does not exist\n");
        return -1;
    }
    int configFD = open(".configure", O_RDONLY);
    off_t currentPos = lseek(configFD, (size_t)0, SEEK_CUR);
    off_t size = lseek(configFD, (size_t)0, SEEK_END);
    lseek(configFD, (size_t)0, SEEK_SET);
    char *fileInfo = (char *)malloc(sizeof(char) * size);
    read(configFD, fileInfo, size);
    char *space = strchr(fileInfo, ' ');
    int spacel = space - fileInfo;
    char *ip = (char *)malloc(spacel + 1);
    char *host = (char *)malloc(size - spacel + 1);

    memcpy(ip, &fileInfo[0], spacel);
    memcpy(host, &fileInfo[spacel + 1], size - spacel + 1);

    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int portNum = atoi(host);
    struct hostent *server = gethostbyname(ip);
    if (sockfd < 0)
    {
        printf("Error opening socket\n");
        return -1;
    }

    if (server == NULL)
    {
        printf("Error: No such host\n");
        return -1;
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portNum);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Error connecting to host\n");
        return -1;
    }
    write(sockfd, "Established Connection", 22);
    char reading[23];
    read(sockfd, reading, 22);
    printf("%s\n", reading);
    free(fileInfo);
    free(ip);
    free(host);
    return sockfd;
}
/**
 * Connects to the server and sends a message to create a project. 
 * The server then creates the project if it does not exist and if it does exist, returns an error
 * 
 */

void create(char *projectName)
{
    int socketFD = checkConnection();
    if (socketFD == -1)
    {
        return;
    }

    int writeLen = 1 + strlen(projectName);
    char *combined = malloc(writeLen);
    memset(combined, 0, 7);
    strcat(combined, "5");
    strcat(combined, projectName);
    write(socketFD, combined, writeLen);
    char created[1];

    read(socketFD, created, 1);
    int c = atoi(created);
    if (!c)
    {
        printf("Project already exists in the server\n");
        return;
    }
    else
    {
        int sys = mkdir(projectName, 00777);
        printf("Project created on the server\n");
    }
    char *gettingTotalBytes = malloc(1);
    char *i = malloc(5);
    bzero(i, 5);
    int z = 0;
    do
    {
        read(socketFD, gettingTotalBytes, 1);
        z++;
        if (gettingTotalBytes[0] != ' ')
        {
            strcat(i, gettingTotalBytes);
        }
    } while (gettingTotalBytes[0] != ' ');
    int totalBytes = atoi(i);
    char *string = malloc(11 + strlen(projectName));
    bzero(string, 11 + strlen(projectName));
    strcat(string, projectName);
    strcat(string, "/.manifest");
    printf("%s\n", string);
    string[strlen(string)] = '\0';
    int manifestFD = open(string, O_CREAT | O_RDWR, 00777);
    int totalBytesRead = 0;
    while (totalBytes > totalBytesRead)
    {
        int bytesToRead = min((totalBytes - totalBytesRead), 2048);
        char *manifest[bytesToRead];
        int bytesRead = read(socketFD, manifest, bytesToRead);
        write(manifestFD, manifest, bytesRead);
        totalBytesRead += bytesRead;
    }
    free(string);
    free(gettingTotalBytes);
    free(i);
    free(combined);
    closeConnection(socketFD);
}

/**
 * Deletes a project repository on the server. Does not change it on the client's machine
 * If connection failed or if the project does not exist, print an error
 */
void destroy(char *projectName)
{
    int serverFD = checkConnection();
    if (serverFD == -1)
    {
        return;
    }
    int prLen = strlen(projectName) + 1;
    //write(serverFD, ("6%s", projectName), (1 + prLen));
    char *combined = malloc(prLen);
    memset(combined, 0, 7);
    strcat(combined, "6");
    strcat(combined, projectName);
    write(serverFD, combined, prLen);

    char passC[1];
    read(serverFD, passC, 1);
    printf("PASSC: %s\n", passC);
    int pass = atoi(passC);
    printf("PASS: %d\n", pass);
    if (pass == 0)
    {
        printf("Project does not exist in the server\n");
    }
    else if (pass == 1)
    {
        printf("Project successfully defeated\n");
    }
    return;
}
/**
 * Gets the current project version and the current file version of all the files inside the project
 * 
 */
void currentVersion(char *projectName)
{
    int socketFD = checkConnection();
    if (socketFD == -1)
    {
        return;
    }
    char* command=malloc(1+strlen(projectName));
    bzero(command,1+strlen(projectName));
    strcat(command,"7");
    strcat(command,projectName);
    write(socketFD,command,strlen(command));
    printf("%s\n",command);
    char created[1];
    read(socketFD, created, 1);
    int c = atoi(created);
    if (!c)
    {
        printf("Project does not exist on the server\n");
        return;
    }
    char *gettingTotalBytes = malloc(1);
    char *i = malloc(10);
    bzero(i, 10);
    //Getting the total size of the file
    do
    {
        read(socketFD, gettingTotalBytes, 1);
        printf("Getting Total Bytes:%s\n",gettingTotalBytes);

        if (gettingTotalBytes[0] != ' ')
        {
            strcat(i, gettingTotalBytes);
        }
    } while (gettingTotalBytes[0] != ' ');
    printf("%s\n",i);
    int totalBytes = atoi(i);
    printf("Total Bytes: %d\n",totalBytes);
    int bytesRead = 0;
    int isFirstLine = 1;
    char byteRead[1];
    char *projectVersion = malloc(10);
    bzero(projectVersion, 10);
    //Getting the project version
    while (isFirstLine)
    {
        bytesRead += read(socketFD, byteRead, 1);
        if (byteRead[0] == '\n')
        {
            isFirstLine = 0;
            printf("Project Version:%s\n", projectVersion);
        }
        else
        {
            strcat(projectVersion, byteRead);
        }
        bytesRead += read(socketFD, byteRead, 1);
    }
    free(projectVersion);
    int readingFile=1;
    int readingVersion=0;
    char* fileName=malloc(1);
    char* versionName=malloc(1);
    //Getting the version of each file
    while (bytesRead < totalBytes)
    {
        bytesRead+=(socketFD,byteRead,1);
        if(readingFile){
            if(byteRead[0]==' '){
                printf("%s: ",fileName);
                fileName=0;
                readingVersion=1;
                fileName=realloc(fileName,1);
            }
            else{
                char* temp=malloc(strlen(fileName)+1);
                bzero(temp,strlen(fileName)+1);
                strcat(temp,fileName);
                strcat(temp,byteRead);
                fileName=realloc(fileName,strlen(temp));
                bzero(fileName,strlen(fileName));
                strcpy(fileName,temp);
                free(temp);
            }
        }
        else if(readingVersion){
            if(byteRead[0]==' '){
                printf("%s\n",versionName);
                readingVersion=0;
                versionName=realloc(versionName,1);
            }
            else{
                char* temp=malloc(strlen(versionName)+1);
                bzero(temp,strlen(versionName)+1);
                strcat(temp,versionName);
                strcat(temp,byteRead);
                versionName=realloc(versionName,strlen(temp));
                bzero(versionName,strlen(versionName));
                strcpy(versionName,temp);
                free(temp);
            }
        }
        else if(byteRead[0]=='\n'){
            readingFile=1;
            readingVersion=0;
        }
    }
    free(fileName);
    free(versionName);
    close(socketFD);
    return;
}

/**
 *  Takes in a project name and a file name and adds it to the manifest
 */

void add(char *projectName, char *fileName)
{
    DIR *dir = opendir(projectName);
    if (dir)
    {
        /* Directory exists. */
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        printf("Directory does not exist\n");
    }
    else
    {
        /* opendir() failed for some other reason. */
    }
    char *manifestPath = malloc(10 + strlen(projectName));
    memset(manifestPath, 0, 10 + strlen(projectName));
    strcat(manifestPath, projectName);
    strcat(manifestPath, "/.manifest");
    int manifestFD = open(manifestPath, O_RDWR, 00777);
    int bytesRead = 1;
    char *fileNameRead = malloc(2);
    memset(fileNameRead, 0, 2);
    fileNameRead[1] = '\0';
    char *totalBytesRead = malloc(1);
    int readFile = 0;
    char *filePath = calloc(strlen(projectName) + strlen(fileName) + 2, sizeof(char));
    strcpy(filePath, projectName);
    strcat(filePath, "/");
    strcat(filePath, fileName);
    if (access(filePath, F_OK) == -1)
    {
        printf("Error:File does not exist in the project\n");
        return;
    }

    while (bytesRead > 0)
    {
        bytesRead = read(manifestFD, fileNameRead, 1);
        if (readFile && fileNameRead[0] == ' ')
        {
            printf("%s\n", totalBytesRead);
            if (strcmp(totalBytesRead, filePath) == 0)
            {
                printf("Warning, the file already exists in the manifest\n");
                return;
            }
            readFile = 0;
        }
        else if (fileNameRead[0] == ' ')
        {
        }
        else if (fileNameRead[0] == '\n')
        {
            readFile = 1;
            totalBytesRead = realloc(totalBytesRead, 1);
            memset(totalBytesRead, 0, 1);
        }
        else if (readFile)
        {
            char *temp = malloc(strlen(totalBytesRead) + 1);
            strcpy(temp, totalBytesRead);
            strcat(temp, fileNameRead);
            totalBytesRead = realloc(totalBytesRead, strlen(temp) + 1);
            memset(totalBytesRead, 0, strlen(temp) + 1);
            strcpy(totalBytesRead, temp);
            free(temp);
        }
    }
    close(manifestFD);
    manifestFD = open(manifestPath, O_RDWR | O_APPEND, 00777);
    printf("%s\n", filePath);
    int fileFD = open(filePath, O_RDONLY);
    off_t currentPos = lseek(fileFD, (size_t)0, SEEK_CUR);
    off_t size = lseek(fileFD, (size_t)0, SEEK_END);
    lseek(fileFD, (size_t)0, SEEK_SET);
    char *fileContent = malloc(size + 1);
    read(fileFD, fileContent, size);
    fileContent[size] = '\0';
    unsigned char *result = malloc(MD5_DIGEST_LENGTH);
    char *s2 = "hello";
    MD5(fileContent, fileFD, result);
    int i;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        printf("%x", result[i]);
    }
    printf("\n");
    char str[50];
    sprintf(str, "%02x", *(uint32_t *)result);
    printf("%s\n", str);
    write(manifestFD, filePath, strlen(filePath));
    write(manifestFD, " $A ", 4);
    write(manifestFD, str, strlen(str));
    write(manifestFD, "\n", 1);
    free(filePath);
    free(fileContent);
    free(totalBytesRead);
}

/** Creates the .configure file given the ip/hostname and the port of the server.
 *  Prints a warning if the file exists and is overwritten (If we can do this)
 *  Does not connect to the server.
 */
void configure(char *ip, char *port)
{
    int configFD = open(".configure", O_RDWR | O_CREAT | O_TRUNC, 00777);
    write(configFD, ip, strlen(ip));
    write(configFD, " ", 1);
    write(configFD, port, strlen(port));
    close(configFD);
    return;
}

/**
 * Closes the connection with the server. IDK if this is all we have to do
 * 
 */
int closeConnection(int serverFD)
{
    close(serverFD);
    printf("Connection closed \n");
}

int main(int argc, char *argv[])
{
    if (argc == 1 || argc == 2)
    {
        printf("Error: Invalid Arguments\n");
        return 1;
    }
    if (argc == 3)
    {
        if (strcmp("Checkout", argv[1]) == 0)
        {
        }
        else if (strcmp("Update", argv[1]) == 0)
        {
        }
        else if (strcmp("Upgrade", argv[1]) == 0)
        {
        }
        else if (strcmp("Commit", argv[1]) == 0)
        {
        }
        else if (strcmp("Push", argv[1]) == 0)
        {
        }
        else if (strcmp("Create", argv[1]) == 0)
        {
            create(argv[2]);
        }
        else if (strcmp("Destroy", argv[1]) == 0)
        {
            destroy(argv[2]);
        }
        else if (strcmp("CurrentVersion", argv[1]) == 0)
        {
            currentVersion(argv[2]);
        }
        else if (strcmp("History", argv[1]) == 0)
        {
        }
        else
        {
            printf("Error: Invalid Arguments\n");
            return 1;
        }
    }
    else if (argc == 4)
    {
        if (strcmp("Configure", argv[1]) == 0)
        {
            configure(argv[2], argv[3]);
            return 1;
        }
        else if (strcmp("Add", argv[1]) == 0)
        {
            add(argv[2], argv[3]);
        }
        else if (strcmp("Rollback", argv[1]) == 0)
        {
        }
        else
        {
            printf("Error: Invalid Arguments\n");
            return 1;
        }
    }
    else
    {
        printf("Error:Invalid Arguments\n");
        return 1;
    }
    return 0;
}

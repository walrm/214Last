#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h> 

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
    off_t currentPos = lseek(".configure", (size_t)0, SEEK_CUR);
    int size = lseek(configFD, (size_t)0, SEEK_END);
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
        printf("ERROR opening socket\n");
    }
    if (server == NULL)
    {
        printf("ERROR, no such host\n");
        return -1;
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portNum);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
        printf("Error connecting to host");
    }
    free(fileInfo);
    free(ip);
    free(host);
    return sockfd;   

}






/** Creates the .configure file given the ip/hostname and the port of the server.
 *  Prints a warning if the file exists and is overwritten (If we can do this)
 *  Does not connect to the server.
 */
void configure(char *ip, char *port)
{
    int configFD = open(".configure", O_RDWR | O_CREAT, 00777);
    write(configFD, ip, strlen(ip));
    write(configFD, " ", 1);
    write(configFD, port, strlen(port));
    close(configFD);
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
        }
        else if (strcmp("Destroy", argv[1]) == 0)
        {
        }
        else if (strcmp("CurrentVersion", argv[1]) == 0)
        {
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
            checkConnection();
        }
        else if (strcmp("Add", argv[1]) == 0)
        {
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
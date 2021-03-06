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
#include<sys/wait.h> 
#include <errno.h>

/**
 * $M-File is changed 2
 * $R-File is deleted 3
 * $A-File is added 1
 * 0-Create
 * 
 */

typedef struct _node
{
    char *fileName;
    char *hash;
    char *liveHash;
    int code; //0:Nothing,1:Add,2:Modify,3:Delete
    int version;
    struct _node *next;
} Node;

typedef struct _manifest
{
    int manifestVersion;
    Node *files;
} Manifest;

/**Returns the file size given the file descriptor
 * does not close the file
 * 
 * @param fd The file descriptor of the file
 * @return the size of the file
 */
int getFileSize(int fd)
{
    off_t currentPos = lseek(fd, (size_t)0, SEEK_CUR);
    off_t size = lseek(fd, (size_t)0, SEEK_END);
    lseek(fd, (size_t)0, SEEK_SET);
    return size;
}
/**Gets a string from an integer
 * The string has to be freed;
 */
char *itoa(int num)
{
    int length = snprintf(NULL, 0, "%d", num);
    char *str = calloc(length + 1, 1);
    sprintf(str, "%d", num);
    return str;
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
    unsigned char result[MD5_DIGEST_LENGTH];
    char *filePath = calloc(strlen(projectName) + strlen(ptr->fileName) + 2, 1);
    strcat(filePath, ptr->fileName);
    int fd = open(filePath, O_RDWR, 00777);
    int fileSize = getFileSize(fd);
    char *file_buffer = calloc(fileSize + 1, 1);
    read(fd, file_buffer, fileSize);
    MD5(file_buffer, fileSize, result);
    int i = 0;
    char *str = calloc(33, 1);
    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(&str[i * 2], "%02x", (unsigned int)result[i]);
    }
    close(fd);
    free(file_buffer);
    free(filePath);
    return str;
}
/**
 * Creates a manifest struct given the fileDescriptor to read from that starts with the first byte of the manifest file,
 * the total bytes to read
 * the name of the project of the manifest file
 * and whether the manifest file is on the client side or the server side
 * @param fd the file descriptor to read from
 * @param totalBytes the total bytes to read from the fileDescriptor
 * @param projectName the name of the project that the commit/manifest file is for
 * @param isLocal 1 if the project is local, 0 otherwise
 * @param isManifestFile 1 if it is from a .Manifest file or a .Update file, 0 if it is a .Conflict, or .Commit file
 * @return the manifest file, with each file being held in a Node*
 */
Manifest *createManifestStruct(int fd, int totalBytes, char *projectName, int isLocal, int isManifestFile)
{
    //printf("Creating Manifest Struct\n");
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
                else if (strcmp(name, "$R") == 0 || strcmp(name, "$D") == 0)
                { //Remove
                    ptr->code = 3;
                }
                else
                {
                    ptr->code = 0;
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
                //printf("Version:%s\n", name);
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
                boolean = 1;
                ptr->next = NULL;
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
                if (strcmp(ptr->liveHash, ptr->hash) != 0 && ptr->code != 1)
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

void freeFileStruct(Node *head)
{
    if (head == NULL)
    {
        return;
    }
    if (head->next != NULL)
    {
        freeFileStruct(head->next);
    }
    if (head->fileName != NULL)
    {
        free(head->fileName);
    }
    free(head->hash);
    free(head->liveHash);
    free(head);
    head = NULL;
}
void freeManifestStruct(Manifest *man)
{
    if (man->files != NULL)
        freeFileStruct(man->files);
    free(man);
}
int min(int a, int b)
{
    if (a > b)
    {
        return b;
    }
    return a;
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
    int size = getFileSize(configFD);
    char *fileInfo = calloc(size + 1, 1);
    //fileInfo="";
    read(configFD, fileInfo, size);
    char *space = strchr(fileInfo, ' ');
    int spacel = space - fileInfo;
    char *ip = (char *)calloc(spacel + 1, 1);
    char *host = (char *)calloc(size - spacel + 1, 1);
    memcpy(ip, &fileInfo[0], spacel);
    memcpy(host, &fileInfo[spacel + 1], size - spacel + 1);

    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int portNum = atoi(host);
    struct hostent *server = NULL;
    server = gethostbyname(ip);
    if (sockfd < 0)
    {
        printf("Error opening socket\n");
        free(ip);
        free(host);
        free(fileInfo);
        return -1;
    }

    if (server == NULL)
    {
        printf("Error: No such host\n");
        free(ip);
        free(host);
        free(fileInfo);
        return -1;
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portNum);
    while (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Connecting to host...\n");
        sleep(3);
    }
    write(sockfd, "Established Connection", 22);
    char *reading = calloc(23, 1);
    read(sockfd, reading, 22);
    printf("%s\n", reading);
    free(reading);
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

    char *combined = calloc(strlen(projectName) + 1, 1);
    strcat(combined, "5");
    strcat(combined, projectName);
    write(socketFD, combined, strlen(combined));
    char *created = calloc(2, 1);

    read(socketFD, created, 1);
    int c = atoi(created);
    free(created);
    if (!c)
    {
        closeConnection(socketFD);
        free(combined);
        printf("Project already exists in the server\n");
        return;
    }
    else
    {
        int sys = mkdir(projectName, 00777);
        printf("Project created on the server\n");
    }
    //Getting the size of the manifest file
    char *gettingTotalBytes = malloc(2);
    bzero(gettingTotalBytes, 2);
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
    //Creating the manifest file
    char *string = malloc(11 + strlen(projectName));
    bzero(string, 11 + strlen(projectName));
    strcat(string, projectName);
    strcat(string, "/.Manifest");
    printf("%s\n", string);
    string[strlen(string)] = '\0';
    int manifestFD = open(string, O_CREAT | O_RDWR, 00777);
    int totalBytesRead = 0;
    while (totalBytes > totalBytesRead)
    {
        int bytesToRead = min((totalBytes - totalBytesRead), 500);
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
    char *combined = calloc(strlen(projectName) + 2, 1);
    strcat(combined, "6");
    strcat(combined, projectName);
    write(serverFD, combined, strlen(combined));
    free(combined);
    char *passC = calloc(2, 1);
    //passC="";
    read(serverFD, passC, 1);
    int pass = atoi(passC);
    free(passC);
    if (pass == 0)
    {
        printf("Project does not exist in the server\n");
    }
    else if (pass == 1)
    {
        printf("Project successfully deleted\n");
    }
    closeConnection(serverFD);
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
    char *command = malloc(1 + strlen(projectName));
    bzero(command, 1 + strlen(projectName));
    strcat(command, "7");
    strcat(command, projectName);
    write(socketFD, command, strlen(command));
    free(command);
    char created[1];
    read(socketFD, created, 1);
    int c = atoi(created);
    if (!c)
    {
        printf("Project does not exist on the server\n");
        return;
    }
    char *gettingTotalBytes = calloc(2, 1);
    char *i = malloc(10);
    bzero(i, 10);
    //Getting the total size of the file
    do
    {
        read(socketFD, gettingTotalBytes, 1);
        if (gettingTotalBytes[0] != ' ')
        {
            strcat(i, gettingTotalBytes);
        }
    } while (gettingTotalBytes[0] != ' ');
    int totalBytes = atoi(i);
    int bytesRead = 0;
    int isFirstLine = 1;
    char *byteRead = malloc(1);
    char *projectVersion = malloc(10);
    bzero(projectVersion, 10);
    //Getting the project version
    while (isFirstLine)
    {
        free(byteRead);
        byteRead = calloc(2, 1);
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
    }

    //Getting the version of each file
    int readingFile = 1;
    int readingVersion = 0;
    char *fileName = malloc(1);
    char *versionName = malloc(1);
    bzero(fileName, 1);
    bzero(versionName, 1);
    char *fileByteRead = malloc(1);
    while (bytesRead < totalBytes)
    {
        free(fileByteRead);
        fileByteRead = malloc(2);
        bzero(fileByteRead, 2);
        bytesRead += read(socketFD, fileByteRead, 1);
        fileByteRead[1] = '\0';
        if (readingFile)
        {
            if (fileByteRead[0] == ' ')
            {
                printf("%s \t", fileName);
                readingFile = 0;
                readingVersion = 1;
                fileName = realloc(fileName, 1);
                bzero(fileName, 1);
            }
            else
            {
                char *temp = malloc(strlen(fileName) + 1);
                bzero(temp, strlen(fileName) + 1);
                strcat(temp, fileName);
                strcat(temp, fileByteRead);
                fileName = realloc(fileName, strlen(temp));
                bzero(fileName, strlen(fileName));
                strcpy(fileName, temp);
                free(temp);
            }
        }
        else if (readingVersion)
        {
            if (fileByteRead[0] == ' ')
            {
                printf("%s\n", versionName);
                readingVersion = 0;
                readingFile = 0;
                versionName = realloc(versionName, 1);
                bzero(versionName, 1);
            }
            else
            {
                char *temp = malloc(strlen(versionName) + 1);
                bzero(temp, strlen(versionName) + 1);
                strcat(temp, versionName);
                strcat(temp, fileByteRead);
                versionName = realloc(versionName, strlen(temp) + 1);
                bzero(versionName, strlen(versionName));
                strcpy(versionName, temp);
                free(temp);
            }
        }
        else if (fileByteRead[0] == '\n')
        {
            readingFile = 1;
            readingVersion = 0;
        }
        else
        {
        }
    }
    free(projectVersion);
    free(byteRead);
    free(i);
    free(versionName);
    free(fileName);
    free(fileByteRead);
    free(gettingTotalBytes);
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
        return;
    }
    else
    {
        printf("Error accessing directory\n");
    }
    char *manifestPath = malloc(11 + strlen(projectName));
    memset(manifestPath, 0, 11 + strlen(projectName));
    strcat(manifestPath, projectName);
    strcat(manifestPath, "/.Manifest\0");
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
        close(manifestFD);
        free(totalBytesRead);
        free(filePath);
        free(fileNameRead);
        free(manifestPath);
        return;
    }

    while (bytesRead > 0)
    {
        bytesRead = read(manifestFD, fileNameRead, 1);
        if (readFile && fileNameRead[0] == ' ')
        {
            if (strcmp(totalBytesRead, filePath) == 0)
            {
                printf("Warning, the file already exists in the manifest\n");
                close(manifestFD);
                free(totalBytesRead);
                free(filePath);
                free(fileNameRead);
                free(manifestPath);
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
            char *temp = malloc(strlen(totalBytesRead) + strlen(fileNameRead) + 1);
            strcpy(temp, totalBytesRead);
            strcat(temp, fileNameRead);
            totalBytesRead = realloc(totalBytesRead, strlen(temp) + 1);
            memset(totalBytesRead, 0, strlen(temp) + 1);
            strcpy(totalBytesRead, temp);
            free(temp);
        }
    }
    close(manifestFD);
    free(totalBytesRead);
    manifestFD = open(manifestPath, O_RDWR | O_APPEND, 00777);
    int fileFD = open(filePath, O_RDONLY);
    int size = getFileSize(fileFD);
    char *fileContent = calloc(size + 1, 1);
    read(fileFD, fileContent, size);
    fileContent[size] = '\0';
    unsigned char *result = calloc(MD5_DIGEST_LENGTH, 1);
    MD5(fileContent, size, result);
    int i;
    char str[50];
    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(&str[i * 2], "%02x", (unsigned int)result[i]);
    }
    write(manifestFD, projectName, strlen(projectName));
    write(manifestFD, "/", 1);
    write(manifestFD, fileName, strlen(fileName));
    write(manifestFD, " $A ", 4);
    write(manifestFD, "0 ", 2);
    write(manifestFD, str, strlen(str));
    write(manifestFD, "\n", 1);
    close(fileFD);
    close(manifestFD);
    free(fileNameRead);
    free(filePath);
    free(fileContent);
    free(result);
    free(manifestPath);
}

/**
 * Writes a message in the .Manifest to delete a file. 
 * If it does not exist in the .manifest, will print an error. 
 * The project needs to exist on the client, but the file does not. 
 * Does not connect to the server.
 * @param projectName name of the project to search for file
 * @param fileName Name of the file to delete
 */
void removeFile(char *projectName, char *fileName)
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
        return;
    }
    else
    {
        printf("Error accessing directory\n");
        return;
    }
    char *manifestPath = malloc(11 + strlen(projectName));
    memset(manifestPath, 0, 11 + strlen(projectName));
    strcat(manifestPath, projectName);
    strcat(manifestPath, "/.Manifest\0");
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
    //printf("%s\n",filePath);
    while (bytesRead > 0)
    {
        bytesRead = read(manifestFD, fileNameRead, 1);
        if (readFile && fileNameRead[0] == ' ')
        {

            if (strcmp(totalBytesRead, filePath) == 0)
            {
                printf("File Removed\n");
                write(manifestFD, "$R", 2);
                close(manifestFD);
                free(totalBytesRead);
                free(filePath);
                free(fileNameRead);
                free(manifestPath);
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
            char *temp = malloc(strlen(totalBytesRead) + strlen(fileNameRead) + 1);
            strcpy(temp, totalBytesRead);
            strcat(temp, fileNameRead);
            totalBytesRead = realloc(totalBytesRead, strlen(temp) + 1);
            memset(totalBytesRead, 0, strlen(temp) + 1);
            strcpy(totalBytesRead, temp);
            free(temp);
        }
    }
    printf("File not found in manifest\n");
    close(manifestFD);
    free(totalBytesRead);
    free(fileNameRead);
    free(filePath);
    free(manifestPath);
}

/** Creates the .configure file given the ip/hostname and the port of the server.
 *  Prints a warning if the file exists and is overwritten (If we can do this)
 *  Does not connect to the server.
 */
void configure(char *ip, char *port)
{
    int configFD = open("./.configure", O_RDWR | O_CREAT | O_TRUNC, 00777);
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

/**
 * Helper method to get the total bytes to read for a file from the server
 * Reads until it finds a space, and then converts the char* to an int
 * @param socketFD the fileDescriptor for the socket
 * @return the number of bytes for the file
 */
int getTotalBytes(int socketFD)
{
    char *gettingTotalBytes = calloc(2, 1);
    char *i = calloc(15, 1);
    do
    {
        read(socketFD, gettingTotalBytes, 1);
        if (gettingTotalBytes[0] != ' ')
        {
            strcat(i, gettingTotalBytes);
        }
    } while (gettingTotalBytes[0] != ' ');
    int totalBytes = atoi(i);
    free(gettingTotalBytes);
    free(i);
    return totalBytes;
}
/**
 * Returns the name of the file or directory in a memory allocated char*. 
 * Remember to free the char* in the method that this helper method is called
 * The socket starts at the first character of the name of the file.
 */
char *getFileName(int socketFD)
{
    char *gettingTotalBytes = calloc(2, 1);
    char *name = calloc(1, 1);
    do
    {
        printf("to the top\n");
        read(socketFD, gettingTotalBytes, 1);
        if (gettingTotalBytes[0] != ' ')
        {
            char *temp = calloc(strlen(name) + 2, 1);
            strcpy(temp, name);
            strcat(temp, gettingTotalBytes);
            free(name);
            name = calloc(strlen(temp) + 1, 1);
            strcpy(name, temp);
            printf("Name:%s\n", name);
            free(temp);
        }
    } while (gettingTotalBytes[0] != ' ');
    printf("Directory Name:%s\n", name);
    free(gettingTotalBytes);
    return name;
}
/**
 * Helper method to make the file in the client while getting the data from the server
 * @param socketFD Socket file descriptor 
 * @param filename Filename to create, 
 * @param totalBytes Total bytes to read
 * DOES NOT free the filename inside this method
 */
void makeFile(int socketFD, char *fileName, int totalBytes)
{
    int fileFD = open(fileName, O_CREAT | O_RDWR, 00777);
    int totalBytesRead = 0;
    while (totalBytes > totalBytesRead)
    {
        int bytesToRead = min((totalBytes - totalBytesRead), 1000);
        char *sRead = calloc(bytesToRead, 1);
        printf("%s\n", sRead);
        int bytesRead = read(socketFD, sRead, bytesToRead);

        write(fileFD, sRead, bytesRead);
        free(sRead);
        totalBytesRead += bytesRead;
    }
    close(fileFD);
}

/**
 * Gets a project from the server, with all the files, and creates the project repository on the client's pc
 */
void checkout(char *projectName)
{
    //Makes sure the directory does not exist on the client
    DIR *dir = opendir(projectName);
    if (dir)
    {
        printf("Directory already exists on the client\n");
        closedir(dir);
        return;
    }
    else
    {
        closedir(dir);
    }
    //Connects with the server and sends the command and project name
    int socketFD = checkConnection();
    if (socketFD == -1)
    {
        return;
    }
    //Sending the command to the server
    char *command = calloc(strlen(projectName) + 2, 1);
    strcat(command, "0");
    strcat(command, projectName);
    command[strlen(command)] = '\0';
    write(socketFD, command, strlen(command));
    //Reading the servers response(error or not)
    char *reads = calloc(2, 1);
    reads[1] = '\0';
    read(socketFD, reads, 1);
    int cont = atoi(reads);
    switch (cont)
    {
    case 0:
        printf("Project does not exist on server\n");
        return;
        break;
    case 2:
        printf("Error searching for file\n");
        return;
        break;
    }
    free(command);
    free(reads);
    //Creates the directory for the client
    int tarFileBytes = getTotalBytes(socketFD);
    makeFile(socketFD, "checkout.tar.gz", tarFileBytes);
    system("tar -xzf checkout.tar.gz");
    remove("checkout.tar.gz");
    closeConnection(socketFD);
}

/**
 * Sends a file over the socket.
 * First sends the number of bytes in the file, followed by a space
 * Then sends over the file
 * Closes the file afterwards, and frees everything but the socketFD
 */
void sendFile(int fileFD, int socketFD)
{
    int fileSize = getFileSize(fileFD);
    char *fileSizeS = itoa(fileSize);
    printf("%s\n", fileSizeS);

    fileSizeS = addByteToString(fileSizeS, " ");
    write(socketFD, fileSizeS, strlen(fileSizeS));
    free(fileSizeS);
    int bytesSent = 0;
    while (fileSize > bytesSent)
    {
        int bytesToRead = min((fileSize - bytesSent), 500);
        char *toWrite[bytesToRead];
        int bytesRead = read(fileFD, toWrite, bytesToRead);
        write(socketFD, toWrite, bytesRead);
        bytesSent += bytesRead;
    }
    close(fileFD);
}

/**
 * A useful description
 * 
 */
void commit(char *projectName)
{
    //Makes sure the directory exists on the clients side
    DIR *dir = opendir(projectName);
    if (dir)
    {
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        printf("Directory does not exist\n");
        return;
    }
    //Makes sure that there is no nonempty update file and no conflict file
    char *updateFileCheck = calloc(strlen(projectName) + strlen("/.Update") + 1, 1);
    strcat(updateFileCheck, projectName);
    strcat(updateFileCheck, "/.Update");
    struct stat stat_record;
    if (stat(updateFileCheck, &stat_record))
    {
    }
    else if (stat_record.st_size <= 1)
    {
    }
    else
    {
        printf("Error: .Update file contains data\n");
        return;
    }
    free(updateFileCheck);
    updateFileCheck = calloc(strlen(projectName) + strlen("/.conflict") + 1, 1);
    if (access(updateFileCheck, F_OK) != -1)
    {
        printf("Error: .conflict file exists\n");
        return;
    }
    free(updateFileCheck);
    int socketFD = checkConnection();
    if (socketFD == -1)
    {
        return;
    }
    //Writes to the server the command (3) and the project name (no space in between)
    char *command = calloc(strlen("3") + strlen(projectName) + 1, 1);
    strcat(command, "3");
    strcat(command, projectName);
    write(socketFD, command, strlen(command));
    char *contS = calloc(2, 1);
    read(socketFD, contS, 1);
    int cont = atoi(contS);
    free(contS);
    free(command);
    if (cont == 0)
    {
        closeConnection(socketFD);
        printf("Project does not exist on the server");
        return;
    }

    //Get client manifest and manifest version
    char *clientManifest = calloc(strlen(projectName) + strlen("/.Manifest") + 1, 1);
    strcat(clientManifest, projectName);
    strcat(clientManifest, "/.Manifest");
    int clientManifestFD = open(clientManifest, O_RDWR, 00777);
    int clientManifestSize = getFileSize(clientManifestFD);
    Manifest *clientMan = createManifestStruct(clientManifestFD, clientManifestSize, projectName, 1, 1);
    free(clientManifest);
    char *serverManifestRead = calloc(2, 1);
    serverManifestRead[1] = '\0';
    char *serverManifestSizeS = calloc(10, 1);
    while (serverManifestRead[0] != ' ')
    {
        read(socketFD, serverManifestRead, 1);
        if (serverManifestRead[0] != ' ')
        {
            strcat(serverManifestSizeS, serverManifestRead);
        }
    }
    int serverManifestSize = atoi(serverManifestSizeS);
    free(serverManifestSizeS);
    free(serverManifestRead);
    Manifest *serverMan = createManifestStruct(socketFD, serverManifestSize, projectName, 0, 1);
    //Makes sure the manifest versions match
    if (serverMan->manifestVersion != clientMan->manifestVersion)
    {
        write(socketFD, "0", 1);
        printf("Error:Manifest Versions do not match, update local repository\n");
        closeConnection(socketFD);
        return;
    }
    write(socketFD, "1", 1);
    Node *clientFiles = clientMan->files;
    Node *clientPtr = clientFiles;
    char *commitPath = calloc(strlen(projectName) + strlen("/.Commit") + 1, 1);
    strcat(commitPath, projectName);
    strcat(commitPath, "/.Commit");
    int commitFD = open(commitPath, O_RDWR | O_CREAT, 00777);
    //Creates the client file on the server side
    while (clientPtr != NULL)
    {
        printf("%s\n", clientPtr->fileName);
        if (clientPtr->code != 0)
        {
            write(commitFD, clientPtr->fileName, strlen(clientPtr->fileName));
            write(commitFD, " ", 1);

            switch (clientPtr->code)
            {
            case 1:
                write(commitFD, "$A ", 3);
                printf("A%s\n", clientPtr->fileName);
                break;
            case 2:
                write(commitFD, "$M ", 3);
                printf("M%s\n", clientPtr->fileName);
                clientPtr->version++;
                break;
            case 3:
                write(commitFD, "$D ", 3);
                printf("D%s\n", clientPtr->fileName);
                break;
            default:
                break;
            }
            char *version = itoa(clientPtr->version);
            write(commitFD, version, strlen(version));
            free(version);
            write(commitFD, " ", 1);
            write(commitFD, clientPtr->liveHash, strlen(clientPtr->liveHash));
            write(commitFD, "\n", 1);
        }
        clientPtr = clientPtr->next;
        //0:Nothing,1:Add,2:Modify,3:Delete
    }
    close(commitFD);
    commitFD = open(commitPath, O_RDONLY);
    sendFile(commitFD, socketFD);
    char *statusS = calloc(2, 1);
    read(socketFD, statusS, 1);
    int status = atoi(statusS);
    if (status)
    {
        printf("Successful commit\n");
    }
    else
    {
        printf("Commit Failed");
        remove(commitPath);
    }
    free(commitPath);
    free(statusS);
    freeManifestStruct(clientMan);
    freeManifestStruct(serverMan);
    closeConnection(socketFD);
    close(clientManifestFD);
}

/**
 * Pushes the code to the server. The client should forward a compressed version of all the files that need to be updated.
 * @param projectName name of the project the push is being done on.
 * Prints an error if the project does not exist on the server, or if no matching commit file exists
 * 
 */
void push(char *projectName)
{
    //Makes sure the project exists on the client
    DIR *dir = opendir(projectName);
    if (dir)
    {
        closedir(dir);
    }
    else if (ENOENT == errno)
    {
        printf("Project does not exist\n");
        return;
    }
    char *commitFilePath = calloc(strlen(projectName) + strlen("/.Commit") + 1, 1);
    sprintf(commitFilePath, "%s/.Commit", projectName);
    //Makes sure the commit file exists on the client
    if (access(commitFilePath, F_OK) == -1)
    {
        //.Commit file does not exist for specified project
        printf("Commit file does not exist, commit before pushing");
        return;
    }
    int socketFD = checkConnection();
    if (socketFD == -1)
    {
        return;
    }
    //Sending the command and the project name
    char *command = calloc(strlen(projectName) + 2, 1);
    strcat(command, "4");
    strcat(command, projectName);
    command[strlen(command)] = '\0';
    write(socketFD, command, strlen(command));
    //write(socketFD, " ", 1);
    free(command);
    //Making sure the project exists on the server
    char *response1C = calloc(2, 1);
    response1C[1] = '\0';
    read(socketFD, response1C, 1);
    int projectExistsOnServer = atoi(response1C);
    if (!projectExistsOnServer)
    {
        printf("The project does not exist on the server\n");
        closeConnection(socketFD);
        free(response1C);
        return;
    }
    free(response1C);
    //Sends the commit file to the server
    int commitFD = open(commitFilePath, O_RDONLY);
    sendFile(commitFD, socketFD);

    //Server sends whether there is a matching commit file or not
    response1C = calloc(2, 1);
    response1C[1] = '\0';
    read(socketFD, response1C, 1);
    int commitExistsOnServer = atoi(response1C);
    if (!commitExistsOnServer)
    {
        printf("No commit file on the server matches the local commit file. Please commit again\n");
        closeConnection(socketFD);
        free(response1C);
        return;
    }
    free(response1C);
    //Creates the file struct from the commit file
    close(commitFD);
    commitFD = open(commitFilePath, O_RDONLY, 00777);
    int commitSize = getFileSize(commitFD);
    Manifest *commitMan = createManifestStruct(commitFD, commitSize, projectName, 0, 0);
    printf("Created Manifest Struct\n");
    Node *ptr = commitMan->files;

    //Creates the system command to tar the files given the list of files that are changed/added
    char *name = calloc(strlen("tar -czvf archive.tar.gz ") + 1, 1);
    strcat(name, "tar -czvf archive.tar.gz ");
    while (ptr != NULL)
    {
        if (ptr->code != 3)
        {
            name = addByteToString(name, ptr->fileName);
            name = addByteToString(name, " ");
        }
        ptr = ptr->next;
    }
    int x = system(name);
    //sends the tarred file to the server
    int tarFD = open("archive.tar.gz", O_RDONLY, 00777);
    sendFile(tarFD, socketFD);
    close(tarFD);
    remove("archive.tar.gz"); //deletes the tarred file
    //updates the manifest
    char *manifestPath = calloc(strlen("/.Manifest") + strlen(projectName) + 1, 1);
    strcat(manifestPath, projectName);
    strcat(manifestPath, "/.Manifest");
    remove(manifestPath);
    int manifestSize = getTotalBytes(socketFD);
    makeFile(socketFD, manifestPath, manifestSize);
    int manifestFD = open(manifestPath, O_CREAT, O_RDWR, 00777);
    free(manifestPath);
    remove(commitFilePath);
    free(commitFilePath);
    free(name);
    freeManifestStruct(commitMan);
    closeConnection(socketFD);
}

void update(char *projectName)
{
    //Makes sure the directory does not exist on the client
    DIR *dir = opendir(projectName);
    if (dir)
    {
        closedir(dir);
    }
    else
    {
        printf("Error: Directory does not exist locally\n");
        return;
    }
    //Connects with the server and sends the command and project name
    int socketFD = checkConnection();
    if (socketFD == -1)
    {
        return;
    }
    char *command = calloc(strlen("1") + strlen(projectName) + 1, 1);
    strcat(command, "1");
    strcat(command, projectName);
    write(socketFD, command, strlen(command));
    free(command);
    char *status = calloc(2, 1);

    //Makes sure the project exists on the server
    read(socketFD, status, 1);
    if (atoi(status) == 0)
    {
        printf("Project does not exist on the server\n");
        free(status);
        closeConnection(socketFD);
    }
    free(status);

    //Creates the linked list for both manifest files
    int serverManifestBytes = getTotalBytes(socketFD);
    Manifest *serverMan = createManifestStruct(socketFD, serverManifestBytes, projectName, 0, 1);
    char *manifestPath = calloc(strlen(projectName) + strlen("/.Manifest") + 1, 1);
    sprintf(manifestPath, "%s/.Manifest", projectName);
    int manifestFD = open(manifestPath, O_RDONLY);
    int clientManifestBytes = getFileSize(manifestFD);
    Manifest *clientMan = createManifestStruct(manifestFD, clientManifestBytes, projectName, 1, 1);
    //Compares the manifest versions of the client and the server

    if (clientMan->manifestVersion == serverMan->manifestVersion)
    {
        printf("Up to Date\n");
        freeManifestStruct(serverMan);
        freeManifestStruct(clientMan);
        closeConnection(socketFD);
        return;
    }

    //Gets the file descriptors of the .Update file and the .Conflict file
    char *updateFilePath = calloc(strlen(projectName) + strlen("/.Update") + 1, 1);
    char *conflictFilePath = calloc(strlen(projectName) + strlen("/.Conflict") + 1, 1);
    sprintf(updateFilePath, "%s/.Update", projectName);
    sprintf(conflictFilePath, "%s/.Conflict", projectName);
    remove(updateFilePath);
    remove(conflictFilePath);
    int updateFD = open(updateFilePath, O_RDWR | O_CREAT, 00777);
    char *serverVersion = itoa(serverMan->manifestVersion);
    write(updateFD, serverVersion, strlen(serverVersion));
    free(serverVersion);
    write(updateFD, "\n", 1);
    int conflictFD = open(conflictFilePath, O_RDWR | O_CREAT, 00777);
    //Writes to the .Update and .Conflict files files
    int isConflict = 0;
    Node *serverPtr = serverMan->files;
    Node *clientPtr = clientMan->files;
    Node *clientPrev = NULL;
    Node *serverPrev = NULL;
    int fileFound;
    while (clientPtr != NULL)
    {
        while (serverPtr != NULL)
        {
            if (strcmp(clientPtr->fileName, serverPtr->fileName) == 0)
            {
                if (strcmp(serverPtr->hash, clientPtr->hash) != 0)
                {
                    if (strcmp(clientPtr->hash, clientPtr->liveHash) != 0)
                    {
                        isConflict = 1;
                        char *version = itoa(serverPtr->version);
                        char *temp = calloc(strlen(clientPtr->fileName) + strlen(serverPtr->hash) + strlen("  $M ") + strlen(version) + 2, 1);
                        sprintf(temp, "%s $C %s %s\n", clientPtr->fileName, version, serverPtr->hash);
                        write(conflictFD, temp, strlen(temp));
                        free(version);
                        free(temp);
                    }
                    else
                    {
                        char *version = itoa(serverPtr->version);
                        char *temp = calloc(strlen(clientPtr->fileName) + strlen(serverPtr->hash) + strlen("  $M ") + strlen(version) + 2, 1);
                        sprintf(temp, "%s $M %s %s\n", clientPtr->fileName, version, serverPtr->hash);
                        write(updateFD, temp, strlen(temp));
                        free(version);
                        free(temp);
                    }
                }
                break;
            }
            else
            {
                serverPrev = serverPtr;
                serverPtr = serverPtr->next;
            }
        }
        if (serverPtr == NULL)
        {
            char *version = itoa(clientPtr->version);
            char *temp = calloc(strlen(clientPtr->fileName) + strlen(clientPtr->hash) + strlen("  $M ") + strlen(version) + 2, 1);
            sprintf(temp, "%s $D %s %s\n", clientPtr->fileName, version, clientPtr->hash);
            write(updateFD, temp, strlen(temp));
            free(version);
            free(temp);
        }
        else
        {
            if (serverPrev == NULL)
            {
                serverMan->files = serverPtr->next;
                free(serverPtr->fileName);
                free(serverPtr->hash);
                free(serverPtr->liveHash);
                free(serverPtr);
                //serverPtr=serverMan->files;
            }
            else
            {
                serverPrev->next = serverPtr->next;
                free(serverPtr->fileName);
                free(serverPtr->hash);
                free(serverPtr->liveHash);
                free(serverPtr);
            }
        }
        clientPrev = clientPtr;
        clientPtr = clientPtr->next;
        serverPtr = serverMan->files;
        serverPrev = NULL;
    }
    if (serverPtr != NULL)
    {
        while (serverPtr != NULL)
        {
            char *version = itoa(serverPtr->version);
            char *temp = calloc(strlen(serverPtr->fileName) + strlen(serverPtr->hash) + strlen("  $M ") + strlen(version) + 2, 1);
            sprintf(temp, "%s $A %s %s\n", serverPtr->fileName, version, serverPtr->hash);
            write(updateFD, temp, strlen(temp));
            free(version);
            free(temp);
            serverPtr = serverPtr->next;
        }
    }
    close(conflictFD);
    close(updateFD);
    freeManifestStruct(clientMan);
    freeManifestStruct(serverMan);
    if (isConflict)
    {
        remove(updateFilePath);
        conflictFD = open(conflictFilePath, O_RDONLY);
        Manifest *conflictMan = createManifestStruct(conflictFD, getFileSize(conflictFD), projectName, 0, 0);
        Node *conflictPtr = conflictMan->files;
        while (conflictPtr != NULL)
        {
            printf("C %s\n", conflictPtr->fileName);
            conflictPtr=conflictPtr->next;
        }
        close(conflictFD);
        freeManifestStruct(conflictMan);
    }
    else
    {
        updateFD = open(updateFilePath, O_RDONLY);
        Manifest *updateMan = createManifestStruct(updateFD, getFileSize(updateFD), projectName, 0, 1);
        Node *updatePtr = updateMan->files;
        while (updatePtr != NULL)
        {
            switch (updatePtr->code)
            {
            case 1:
                printf("A %s\n", updatePtr->fileName);
                break;
            case 2:
                printf("M %s\n", updatePtr->fileName);
                break;
            case 3:
                printf("D %s\n", updatePtr->fileName);
            }
            updatePtr = updatePtr->next;
        }
        close(updateFD);
        freeManifestStruct(updateMan);
        remove(conflictFilePath);
    }
    free(manifestPath);
    free(updateFilePath);
    free(conflictFilePath);
    closeConnection(socketFD);
}

/**
 * $M-File is changed 2
 * $R-File is deleted 3
 * $A-File is added 1
 * 0-Create
 * 
 */

void upgrade(char *projectName)
{
    DIR *dir = opendir(projectName);
    if (dir)
    {
        closedir(dir);
    }
    else
    {
        printf("Error: Directory does not exist locally\n");
        return;
    }
    char *updateFilePath = calloc(strlen(projectName) + strlen("/.Update") + 1, 1);
    char *conflictFilePath=calloc(strlen(projectName)+strlen("/.Conflict")+1,1);
    sprintf(updateFilePath, "%s/.Update", projectName);
    sprintf(conflictFilePath,"%s/.Conflict",projectName);
    struct stat conflict_record;
    int conflictFD=open(conflictFilePath,O_RDONLY);
    if(conflictFD!=-1){
        printf("Error: Fix conflicts\n");
        free(updateFilePath);
        free(conflictFilePath);
        return;
    }
    free(conflictFilePath);
    struct stat stat_record;
    if (stat(updateFilePath, &stat_record))
    {
        printf("Update file does not exist\n");
        free(updateFilePath);
        return;
    }
    else if (stat_record.st_size <= 1)
    {
        printf("Update file is empty\n");
        free(updateFilePath);
        return;
    }

    //Connects with the server and sends the command and project name
    int socketFD = checkConnection();
    if (socketFD == -1)
    {
        return;
    }
    char *command = calloc(strlen("2") + strlen(projectName) + 1, 1);
    strcat(command, "2");
    strcat(command, projectName);
    write(socketFD, command, strlen(command));
    free(command);
    char *status = calloc(2, 1);
    read(socketFD, status, 1);

    //Makes sure the project exists on the server
    if (atoi(status) == 0)
    {
        printf("Project does not exist on the server\n");
        free(status);
        closeConnection(socketFD);
    }
    free(status);

    //Sends the update file to the server
    int updateFD = open(updateFilePath, O_RDONLY, 00777);
    sendFile(updateFD, socketFD);
    close(updateFD);

    //Creates the structs for the manifest and update files
    updateFD = open(updateFilePath, O_RDONLY, 00777);
    char *manifestFilePath = calloc(strlen(projectName) + strlen("/.Manifest") + 1, 1);
    sprintf(manifestFilePath, "%s/.Manifest", projectName);
    int manifestFD = open(manifestFilePath, O_RDONLY);
    Manifest *update = createManifestStruct(updateFD, getFileSize(updateFD), "", 0, 1);
    Manifest *man = createManifestStruct(manifestFD, getFileSize(manifestFD), "", 0, 1);
    Node *updatePtr = update->files;
    Node *manptr = man->files;
    man->manifestVersion++;
    //Goes throught both structs, and modifies the struct
    while (updatePtr != NULL)
    {
        if (updatePtr->code == 3)
        {
            //Remove file from the project
            while (manptr->fileName != NULL && strcmp(manptr->fileName, updatePtr->fileName) != 0)
                manptr = manptr->next;
            free(manptr->fileName);
            manptr->fileName = NULL;
            char *systemCall = malloc(strlen(updatePtr->fileName) + 4);
            sprintf(systemCall, "rm %s", updatePtr->fileName);
            int rmStatus = system(systemCall);
            free(systemCall);
        }
        else if (updatePtr->code == 1)
        {
            //NEED TO TEST: add in commit
            Node *newFile = malloc(sizeof(Node));
            newFile->fileName = malloc(strlen(updatePtr->fileName));
            newFile->hash = malloc(strlen(updatePtr->hash));
            newFile->liveHash = malloc(strlen(updatePtr->hash));
            strcpy(newFile->fileName, updatePtr->fileName);
            strcpy(newFile->hash, updatePtr->hash);
            strcpy(newFile->liveHash, newFile->hash);
            newFile->code = 0;
            newFile->version = 0;
            if (manptr == NULL)
            {
                man->files = newFile;
            }
            else
            {
                Node *temp = manptr->next;
                manptr->next = newFile;
                manptr->next->next = temp;
            }
        }
        else
        {
            //Update hash, code, version
            while (manptr->fileName != NULL && strcmp(manptr->fileName, updatePtr->fileName) != 0)
                manptr = manptr->next;
            bzero(manptr->hash, strlen(manptr->hash));
            strcpy(manptr->hash, updatePtr->hash);
            manptr->code = 0;
            manptr->version = updatePtr->version;
        }
        updatePtr = updatePtr->next;
        manptr = man->files;
    }
    close(manifestFD);
    remove(manifestFilePath);
    manifestFD = open(manifestFilePath, O_CREAT | O_RDWR, 00777);
    char *manVersion = itoa(update->manifestVersion);
    printf("new man version and length: %s,%d\n", manVersion, strlen(manVersion));
    write(manifestFD, manVersion, strlen(manVersion));
    write(manifestFD, "\n", 1);
    free(manVersion);
    //Write out new manifest file
    while (manptr != NULL)
    {
        if (manptr->fileName != NULL)
        {
            write(manifestFD, manptr->fileName, strlen(manptr->fileName));
            write(manifestFD, " ", 1);
            write(manifestFD, "$N ", 3);
            char *fileVersion = malloc(sizeof(manptr->version) / 4 + 1);
            sprintf(fileVersion, "%d", manptr->version);
            write(manifestFD, fileVersion, strlen(fileVersion));
            write(manifestFD, " ", 1);
            write(manifestFD, manptr->hash, strlen(manptr->hash));
            write(manifestFD, "\n", 1);
            free(fileVersion);
        }
        manptr = manptr->next;
    }
    //Bytes of the tar file
    int tarTotalBytes = getTotalBytes(socketFD);
    //Get the tar file if bytes is greater than 0
    if (tarTotalBytes != 0)
    {
        makeFile(socketFD, "update.tar.gz", tarTotalBytes);
        system("tar -xzf update.tar.gz");
        remove("update.tar.gz");
    }
    //Free all the structs
    freeManifestStruct(update);
    freeManifestStruct(man);
    close(updateFD);
    remove(updateFilePath);
    free(manifestFilePath);
    free(updateFilePath);
    closeConnection(socketFD);
}
/**
 * Prints out the history of a specific project. Does not require project to exist locally.
 * This includes all adds, modifies, and removes done to the project
 * @param projectName the name of the project
 */
void history(char *projectName)
{
    int socketFD = checkConnection();
    if (socketFD == -1)
    {
        return;
    }
    char *command = calloc(strlen("8") + strlen(projectName) + 1, 1);
    sprintf(command, "8%s", projectName);
    write(socketFD, command, strlen(command));
    free(command);
    char *status = calloc(2, 1);
    read(socketFD, status, 1);
    if (atoi(status) == 0)
    {
        printf("Project not found on the server\n");
        free(status);
        closeConnection(socketFD);
    }
    read(socketFD, status, 1);
    if (atoi(status) == 0)
    {
        printf("No pushes have been done on the given project in the server");
    }
    free(status);
    int totalBytes = getTotalBytes(socketFD);
    printf("%d\n", totalBytes);
    int bytesRead = 0;
    while (bytesRead < totalBytes)
    {
        int bytesToRead = min(totalBytes - bytesRead, 250);
        char *file = calloc(bytesToRead + 1, 1);
        bytesRead += read(socketFD, file, bytesToRead);
        printf("%s", file);
        free(file);
    }
    closeConnection(socketFD);
    return;
}
/**
 * Asks the server to go back to a previous version of a project
 * @param projectName the name of the project
 * @param ver the version of the project to return to
 * Gives an error if the project is not found, or the requested version is higher then the current project version.
 */
void rollback(char *projectName, char *ver)
{
    int socketFD = checkConnection();
    if (socketFD == -1)
    {
        return;
    }
    char *command = calloc(strlen("8") + strlen(projectName) + strlen(ver) + 2, 1);
    sprintf(command, "9%s %s ", ver, projectName);
    write(socketFD, command, strlen(command));
    char *status = calloc(2, 1);
    read(socketFD, status, 1);
    free(command);
    if (atoi(status) == 0)
    {
        printf("Project not found on the server\n");
        free(status);
        closeConnection(socketFD);
        return;
    }
    read(socketFD, status, 1);
    if (atoi(status) == 0)
    {
        printf("The current version of the project is lower then the version inputted");
        free(status);
        closeConnection(socketFD);
        return;
    }
    printf("Rollback Successful\n");
    free(status);
    closeConnection(socketFD);
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
            checkout(argv[2]);
        }
        else if (strcmp("Update", argv[1]) == 0)
        {
            update(argv[2]);
        }
        else if (strcmp("Upgrade", argv[1]) == 0)
        {
            upgrade(argv[2]);
        }
        else if (strcmp("Commit", argv[1]) == 0)
        {
            commit(argv[2]);
        }
        else if (strcmp("Push", argv[1]) == 0)
        {
            push(argv[2]);
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
            history(argv[2]);
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
        else if (strcmp("Remove", argv[1]) == 0)
        {
            removeFile(argv[2], argv[3]);
        }
        else if (strcmp("Rollback", argv[1]) == 0)
        {
            rollback(argv[2], argv[3]);
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
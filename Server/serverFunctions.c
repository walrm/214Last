#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>

#include "serverFunctions.h"

void possibleError(int socket,char* error){
    write(socket,"2",1);
    printf("%s\n",error);
}

//Create command, creates new project in repository with manifest, writes manifest information to client
void create(char* projectName, int socket){
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

            //Let client know there was an error, project already exists with given name
            if(strcmp(currentINode->d_name,projectName)==0){
                write(socket,"0",1); 
                printf("ERROR- Project already exists\n");
                return;
            }

        }
    }while(currentINode!=NULL); //project doesn't exist
    //Creates project with parameter projectName and create the project's manifest file
    if(mkdir(projectName,00777)<0){
        possibleError(socket,"ERROR on creating directory");
        return;
    }

    char* manifestPath = malloc(strlen(projectName)+13);
    bzero(manifestPath,sizeof(manifestPath));
    manifestPath[0] = '.';
    manifestPath[1] = '/';
    strcat(manifestPath,projectName);
    char m[10] = "/.Manifest";
    strcat(manifestPath, m);
    manifestPath[strlen(manifestPath)] = '\0';
    printf("MANIFEST PATH: %s\n", manifestPath);
    int manifestFD = open(manifestPath, O_CREAT | O_RDWR, 00777);
    if(manifestFD<0){
        possibleError(socket,"ERROR on open() for manifest file");
        return;
    }

    write(manifestFD,"0\n",2);  //Writing version number 0 on the first line

    //Write to client that project has been successfully created on server side
    write(socket,"1",1); 
    
    close(manifestFD);
    manifestFD = open(manifestPath, O_RDONLY);
    struct stat manStats;
    if(stat(manifestPath,&manStats)<0){
        possibleError(socket,"ERROR reading manifest stats");
        return;
    }

    //Sending manifest file over by writing to socket
    int size = manStats.st_size; 
    int bytesRead = 0, bytesToRead = 0;
    char manBuffer[256];

    //Send size of manifest file to client
    sprintf(manBuffer,"%d", size);
    write(socket,manBuffer,strlen(manBuffer));
    write(socket," ",1);
    printf("manBuffer: %s\n",manBuffer);

    //Send manifest bytes to client
    while(size > bytesRead){
        bytesToRead = (size-bytesRead<256)? size-bytesRead : 255;
        bzero(manBuffer,256);
        bytesRead += read(manifestFD,manBuffer,bytesToRead);
        write(socket,manBuffer,bytesToRead);
    }
    
    printf("End of create\n");
    free(manifestPath);
    closedir(cwd);
    close(manifestFD);
    return;
}

//Helper function for delete - recursively delete files and directories in project folder
void destroyProject(char* path, int socket){
    DIR *cwd = opendir(path);
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
            
            //Step into directory and delete all directories/files
            char nextPath[PATH_MAX];
            strcpy(nextPath, path);
            strcat(strcat(nextPath, "/"), currentINode->d_name); //appends directory name to path
            destroyProject(nextPath,socket);

            //Find full path of directory and remove after it has been emptied
            char* dirpath = malloc(strlen(path)+strlen(currentINode->d_name)+3);
            if(dirpath == NULL){
                possibleError(socket,"ERROR on malloc");
                return;
            }

            strcpy(dirpath,path);
            strcat(dirpath,"/");
            strcat(dirpath,currentINode->d_name);
            if(rmdir(dirpath)<0){
                possibleError(socket,"ERROR on removing directory");
                return;
            }
            
            free(dirpath);

        //INode is a file
        }else if(currentINode!=NULL){
            char* file = malloc(strlen(path)+strlen(currentINode->d_name)+2);
            if(file == NULL){
                possibleError(socket,"ERROR on malloc");
                return;
            }

            strcpy(file, path);
            strcat(file,"/");
            strcat(file,currentINode->d_name); //appends file name to path
            file[strlen(file)] = '\0';
            printf("FILE PATH: %s\n", file);
            if(remove(file)<0){
               possibleError(socket,"ERROR on unlink/remove file");
                return;
            }

            free(file);
        }
    }while(currentINode!=NULL); 
    closedir(cwd); 
}

//Deletes project folder in repository 
void delete(char* projectName, int socket){
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

            //Found project and delete using sys call
            if(strcmp(currentINode->d_name,projectName)==0){
                char* path = malloc(strlen(projectName)+3);
                if(path == NULL){
                    possibleError(socket,"ERROR on malloc");
                    return;
                }
                path[0] = '.';
                path[1] = '/';
                strcat(path,projectName);
                printf("PATH: %s\n", path);
                destroyProject(path,socket);
                closedir(cwd);
                if(rmdir(path)<0){
                    possibleError(socket,"ERROR on removing project directory");
                    return;
                }

                write(socket,"1",1); //Send message to client that project has been sucessfully deleted
                free(path);
                return;
            }
        }
    }while(currentINode!=NULL); 
    write(socket,"0",1); //project doesn't exist, return error to client
    closedir(cwd);
}

//Returns manifest information for client to read versions
void currentVersion(char* projectName, int socket){
    DIR *cwd = opendir("./");
    struct dirent *currentINode = NULL;
    if(cwd == NULL){
        possibleError(socket,"ERROR on opening directory");
        return;
    }

    do{
        currentINode = readdir(cwd);
        if(currentINode!=NULL && currentINode->d_type == DT_DIR){
            if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                    continue;

            //Project found, writing manifest data to socket
            if(strcmp(currentINode->d_name,projectName)==0){
                write(socket,"1",1); //send to client that project was found
                
                //Find manifest path and open to read data
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
                
                free(manifestPath);
                closedir(cwd);
                close(socket);
                close(manifestFD);
                break;
            }

        }
    }while(currentINode!=NULL); 
    write(socket,"0",1);  //project doesn't exist, report to client
}

//Commit command. Send manifest file over and have client update files and then receive commit file
void commit(char *projectName, int socket)
{
    DIR *cwd = opendir("./");
    if (cwd == NULL)
    {
        possibleError(socket, "ERROR on opening directory");
        return;
    }

    struct dirent *currentINode = NULL;
    do
    {
        currentINode = readdir(cwd);
        if (currentINode != NULL && currentINode->d_type == DT_DIR)
        {
            if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                continue;

            if (strcmp(currentINode->d_name, projectName) == 0)
            {
                write(socket, "1", 1);

                char *manifestPath = malloc(strlen(projectName) + 13);
                bzero(manifestPath, sizeof(manifestPath));
                sprintf(manifestPath, "./%s/.Manifest", projectName);
                printf("MANIFEST PATH: %s\n", manifestPath);
                int manifestFD = open(manifestPath, O_RDONLY);

                //Use stats to read total bytes of manifest file
                struct stat manStats;
                if (stat(manifestPath, &manStats) < 0)
                {
                    possibleError(socket, "ERROR reading manifest stats");
                    return;
                }

                int size = manStats.st_size;
                int bytesRead = 0, bytesToRead = 0;
                char manBuffer[256];
                bzero(manBuffer,256);

                //Send size of manifest file to client
                sprintf(manBuffer, "%d", size);
                write(socket, manBuffer, strlen(manBuffer));
                write(socket, " ", 1);
                printf("manBuffer: %s\n", manBuffer);
                printf("manBuffer size: %d\n", strlen(manBuffer));

                //Send manifest bytes to client
                while (size > bytesRead)
                {
                    bytesToRead = (size - bytesRead < 256) ? size - bytesRead : 255;
                    bzero(manBuffer, 256);
                    bytesRead += read(manifestFD, manBuffer, bytesToRead);
                    printf("MANBUFFER:\n%s\n", manBuffer);
                    write(socket, manBuffer, bytesToRead);
                }
                bzero(manBuffer,256);

                //Receive status and then receive commit file
                int i = 0;
                char *status = malloc(2);

                read(socket, status, 1); //status of commit from client
                int st = atoi(status);
                free(status);
                if (st == 0)
                {
                    free(manifestPath);
                    close(manifestFD);
                    return;
                }

                //Read in commit file if success on client side
                char *commit = malloc(strlen(projectName) + 13);
                sprintf(commit, "./%s/.Commit%d", projectName, i);
                int commitFD = open(commit, O_RDONLY);
                while (commitFD > 0)
                {
                    i++;
                    sprintf(commit, "./%s/.Commit%d", projectName, i);
                    commitFD = open(commit, O_RDONLY);
                }

                commitFD = open(commit, O_CREAT | O_RDWR, 00777);

                char s[2];
                char buffer[256];
                bzero(buffer,256);
                int totalBytes = 0;
                bytesRead = 0;

                do
                {
                    bzero(s, 2);
                    read(socket, s, 1);
                    if (s[0] != ' ')
                        strcat(buffer, s);
                        printf("buffer: %s\n", buffer);
                } while (s[0] != ' ');

                totalBytes = atoi(buffer);

                printf("TOTALBYTES: %d\n", totalBytes);

                //write out commit file from client
                while (bytesRead < totalBytes)
                {
                    bytesToRead = (totalBytes - bytesRead < 256) ? totalBytes - bytesRead : 255;
                    bzero(buffer, 256);
                    bytesRead += read(socket, buffer, bytesToRead);
                    printf("BUFFER READ: %s\n", buffer);
                    write(commitFD, buffer, bytesToRead);
                }

                write(socket, "1", 1); //write to client - commit successfully stored in server side

                free(commit);
                free(manifestPath);
                close(commitFD);
                close(manifestFD);
                closedir(cwd);
                return;
            }
        }
    } while (currentINode != NULL); //project doesn't exist
    write(socket, "0", 1);          //write to client - project does not exist
}

//Sends history file of commits to the client
void history(char* projectName, int socket){
    DIR *cwd = opendir("./");
    struct dirent *currentINode = NULL;
    if(cwd == NULL){
        possibleError(socket,"ERROR on opening directory");
        return;
    }

    do{
        currentINode = readdir(cwd);
        if(currentINode!=NULL && currentINode->d_type == DT_DIR){
            if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                    continue;

            //Project found, writing manifest data to socket
            if(strcmp(currentINode->d_name,projectName)==0){
                write(socket,"1",1); //send to client that project was found
                
                //send history file to client
                char* historyFile = malloc(strlen(projectName)+12);
                sprintf(historyFile, "./%s/.History",projectName);
                int historyFD = open(historyFile, O_RDONLY);
                if(historyFD < 0)
                    write(socket, "0", 1);
                else   
                    write(socket, "1", 1);

                //Use stats to read total bytes of history file
                struct stat histStats;
                if (stat(historyFile, &histStats) < 0)
                {
                    possibleError(socket, "ERROR reading history stats");
                    return;
                }

                int size = histStats.st_size;
                int bytesRead = 0, bytesToRead = 0;
                char buffer[256];
                bzero(buffer,256);

                //Send size of manifest file to client
                sprintf(buffer, "%d", size);
                write(socket, buffer, strlen(buffer));
                write(socket, " ", 1);
                printf("buffer: %s\n", buffer);
                printf("buffer size: %d\n", strlen(buffer));

                //Send manifest bytes to client
                while (size > bytesRead)
                {
                    bytesToRead = (size - bytesRead < 256) ? size - bytesRead : 255;
                    bzero(buffer, 256);
                    bytesRead += read(historyFD, buffer, bytesToRead);
                    printf("BUFFER:\n%s\n", buffer);
                    write(socket, buffer, bytesToRead);
                }

                free(historyFile);
                close(historyFD);
                closedir(cwd);
                return;
            }
        }
    }while(currentINode!= NULL);
    write(socket, "0", 1); //write to client - project not found
    closedir(cwd);
}
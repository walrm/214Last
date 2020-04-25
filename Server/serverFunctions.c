#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>

#include "serverFunctions.h"

//Create command, creates new project in repository with manifest, writes manifest information to client
void create(char* projectName, int socket){
    //opens (repository) directory from root path and looks if project already exists
    DIR *cwd = opendir("./");
    struct dirent *currentINode = NULL;
    do{
        currentINode = readdir(cwd);
        if(currentINode!=NULL && currentINode->d_type == DT_DIR){
            if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                    continue;

            //Let client know there was an error, project already exists with given name
            if(strcmp(currentINode->d_name,projectName)==0){
                write(socket,"0",1); 
                printf("ERROR SENT\n");
                return;
            }

        }
    }while(currentINode!=NULL); //project doesn't exist
    
    write(socket,"1",1); //Write to client that project has been created
    
    //Creates project with parameter projectName and create the project's manifest file
    mkdir(projectName,0700); 
    char* manifestPath = malloc(strlen(projectName)+13);
    manifestPath[0] = '.';
    manifestPath[1] = '/';
    strcat(manifestPath,projectName);
    char m[10] = "/.Manifest";
    strcat(manifestPath, m);
    int manifestFD = open(manifestPath, O_CREAT | O_RDWR, 00777); 
    write(manifestFD,"0\n",1);  //Writing version number 0 on the first line

    struct stat manStats;
    manifestFD = open(manifestPath, O_RDONLY);
    if(stat(manifestPath,&manStats)<0){
        pError("ERROR reading manifest stats");
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
    printf("manBuffer size: %d\n", strlen(manBuffer));

    //Send manifest bytes to client
    while(size > bytesRead){
        bytesToRead = (size-bytesRead<256)? size-bytesRead : 255;
        bzero(manBuffer,256);
        bytesRead += read(manifestFD,manBuffer,bytesToRead);
        write(socket,manBuffer,bytesToRead);
    }
    
    free(manifestPath);
    closedir(cwd);
    close(socket);
    close(manifestFD);
}

// void writeProjPath(char* projectPath, int manifestFD){
//     DIR* cwd = opendir(projectPath);
//     struct dirent *currentINode = NULL;
//     do{
//         currentINode = readdir(cwd);
//         if(currentINode==NULL) return;
//         if(currentINode->d_type == DT_DIR){
//             if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
//                     continue;
//             char nextPath[PATH_MAX];
//             strcpy(nextPath, path);
//             strcat(strcat(nextPath, "/"), currentINode->d_name);
//             writeProjPath(nextPath, manifestFD);
//         }
//     }while(currentINode!=NULL);
// }
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>

#include "serverFunctions.h"

//TODO: writing server side create function; Need to free 3 memory allocations
void create(char* projectName, int socket){
    //opens (repository) directory from root path and looks if project already exists
    char* manifestPath;
    DIR *cwd = opendir("./");
    struct dirent *currentINode = NULL;
    do{
        currentINode = readdir(cwd);
        if(currentINode!=NULL && currentINode->d_type == DT_DIR){
            if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                    continue;
            printf("%s\n",currentINode->d_name);
            //Let client know there was an error, project already exists with name
            if(strcmp(currentINode->d_name,projectName)==0){
                write(socket,"0",1); 
                printf("ERROR SENT\n");
                return;
            }
        }
    }while(currentINode!=NULL); //project doesn't exist
    write(socket,"1",1);
    mkdir(projectName,0700); //creates project with parameter projectName
    manifestPath = malloc(strlen(projectName)+13);
    manifestPath[0] = '.';
    manifestPath[1] = '/';
    strcat(manifestPath,projectName);
    char m[10] = "/.Manifest";
    strcat(manifestPath, m);
    printf("PROJECT PATH to Manifest: %s\n", manifestPath);

    int manifestFD = open(manifestPath, O_CREAT | O_RDWR, 00777); //Create .Manifest file
    write(manifestFD,"0\n",1);  //Writing version number 0 on the first line

    struct stat manStats;
    manifestFD = open(manifestPath, O_RDONLY);
    if(stat(manifestPath,&manStats)<0){
        pError("ERROR reading manifest stats");
    }

    int size = manStats.st_size; //size of manifest file
    int bytesRead = 0, bytesToRead = 0;
    char manBuffer[256];
    while(size > bytesRead){
        bytesToRead = (size-bytesRead<256)? size-bytesRead : 255;
        bzero(manBuffer,256);
        bytesRead += read(manifestFD,manBuffer,bytesToRead);
        write(socket,manBuffer,bytesToRead);
    }
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
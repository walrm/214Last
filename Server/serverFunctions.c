#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>

#include "serverFunctions.h"

//TODO: writing server side create function; Need to free 2 memory allocations
void create(char* projectName, int socket){
    //opens (repository) directory from root path and looks if project already exists
    DIR *cwd = opendir("./");
    struct dirent *currentINode = NULL;
    do{
        currentINode = readdir(cwd);
        if(currentINode!=NULL && currentINode->d_type == DT_DIR){
            if (strcmp(currentINode->d_name, ".") == 0 || strcmp(currentINode->d_name, "..") == 0)
                    continue;
            printf("%s\n",currentINode->d_name);
            if(strcmp(currentINode->d_name,projectName)==0){
                write(socket,"0",1);
                printf("ERROR SENT");
                return;
            }
        }
    }while(currentINode!=NULL); //project doesn't exist
    mkdir(projectName,0700); //creates project with parameter projectName
    char* manifestPath = malloc(strlen(projectName)+13);
    manifestPath[0] = '.';
    manifestPath[1] = '/';
    strcat(manifestPath,projectName);
    // char* projectPath = malloc(strlen(projectName)+3);
    // strcpy(projectPath, manifestPath);
    // printf("PROJECT PATH: %s\n", projectPath);
    char manifest[10] = "/.Manifest";
    strcat(manifestPath,manifest);
    printf("PROJECT PATH to Manifest: %s\n", manifestPath);
    int manifestFD = open(manifestPath, O_CREAT | O_RDWR, 00777); //Create .Manifest file
    write(manifestFD,"0\n",1);  //Writing version number 0 on the first line

    //Send information over to client socket?
    manifestFD = open(manifestPath, O_RDONLY);
    
    
    //writeProjPath(projectPath, manifestFD);
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
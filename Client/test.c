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

int main(int argc, char *argv[])
{
    int x=system("mkdir \"test2\"");
    printf("%d\n",x);
}
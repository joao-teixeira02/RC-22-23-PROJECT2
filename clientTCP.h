#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <netdb.h>
#include <ctype.h>
#include <string.h>

#define SERVER_PORT 21
#define SERVER_ADDR "ftp.up.pt"
#define URL "ftp://anonymous:@ftp.up.pt/debian/README"
#define MAX_IP_LENGTH 20
#define MAX_LENGTH 256

struct url_args {
    char host[512];
    char user[256];
    char password[256];
    char path[512];
    char filename[512];
    char host_name[512];
    char ip[MAX_IP_LENGTH];
};

struct socket_response {
    char response[1024];
    char code[3];
};
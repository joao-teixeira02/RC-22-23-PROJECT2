/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */

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
#define MAX_IP_LENGTH 16

char* get_ip() {
    struct hostent *h;
    char* ip = (char*) malloc(MAX_IP_LENGTH);
    memset(ip, 0, MAX_IP_LENGTH);
    if ((h = gethostbyname(SERVER_ADDR)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }
    strcpy(ip, inet_ntoa(*((struct in_addr *) h->h_addr)));
    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n", ip);

    return ip;
}

int create_connect_socket(char *ip) {
    int sockfd;
    struct sockaddr_in server_addr;
    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(SERVER_PORT);

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }

    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    return sockfd;
}

int main(int argc, char **argv) {

    if (argc > 1)
        printf("**** No arguments needed. They will be ignored. Carrying ON.\n");
    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
    size_t bytes;
    /*send a string to the server*/
    char* ip = (char*) malloc(MAX_IP_LENGTH);
    strcpy(ip, get_ip());
    int sockfd = create_connect_socket(ip);
    bytes = write(sockfd, buf, strlen(buf));
    if (bytes > 0)
        printf("Bytes escritos %ld\n", bytes);
    else {
        perror("write()");
        exit(-1);
    }

    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }
    return 0;
}

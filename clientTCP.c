/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */

#include "clientTCP.h"

char * get_filename(char * path) {
    char * aux;
    char * token = strtok(path, "/");
   
    while(token != NULL) {
        aux = token;
        token = strtok(NULL, "/");
    }

    return aux;
}

int parse_input_url(char * url, struct url_args * parsed_args) {
    printf("Received url: %s\n", url);

    char * ftp = strtok(url, ":");
    char * args = strtok(NULL, "/");
    char * path = strtok(NULL, "");

    if (ftp == NULL || args == NULL || path == NULL) {
        fprintf(stderr, "Invalid URL!\n");
        return -1;
    }
    
    if (strchr(args, '@') != NULL) {
        char * login = strtok(args, "@");
        char * host = strtok(NULL, "@");

        char * user = strtok(login, ":");
        char * password = strtok(NULL, ":");

        strcpy(parsed_args->user, user);

        if (password == NULL)
            strcpy(parsed_args->password, "pass");
        else
            strcpy(parsed_args->password, password);

        strcpy(parsed_args->host, host);
    }
    else { //if it has no username and password then logs in as anonymous
        strcpy(parsed_args->user, "anonymous");
        strcpy(parsed_args->password, "pass");
        strcpy(parsed_args->host, args);
    }

    char * filename = get_filename(path);
    strcpy(parsed_args->path, path);
    strcpy(parsed_args->filename, filename);

    if (!strcmp(parsed_args->host, "") || !strcmp(parsed_args->path, "")) {
        fprintf(stderr, "Invalid URL!\n");
        return -1;
    }

    struct hostent * h;

    if ((h = gethostbyname(parsed_args->host)) == NULL) {  
        herror("gethostbyname");
        return -1;
    }

    char * host_name = h->h_name;
    strcpy(parsed_args->host_name, host_name);

    char * ip = inet_ntoa(*((struct in_addr *)h->h_addr));
    strcpy(parsed_args->ip, ip);

    printf("\nUser: %s\n", parsed_args->user);
    printf("Password: %s\n", parsed_args->password);
    printf("Host: %s\n", parsed_args->host);
    printf("Path: %s\n", parsed_args->path);
    printf("File name: %s\n", parsed_args->filename);
    printf("Host name  : %s\n", parsed_args->host_name);
    printf("IP Address : %s\n\n", parsed_args->ip);
    return 0;
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

int write_to_socket(int sockfd, int hasBody, char *header, char *body) {
    char message[MAX_LENGTH];
	strcpy(message, header);

	if (hasBody) {
		strcat(message, " ");
		strcat(message, body);
	}

	strcat(message, "\r\n");

    int bytesSent = write(sockfd, message, strlen(message));
    if (bytesSent != strlen(message)) {		
        fprintf(stderr, "Error writing message to socket!\n");
        return -1;
    }

	printf("> %s", message);

    return bytesSent;
}

int read_response(int sockfd, struct socket_response * response) {
    strcpy(response->response, "");

	FILE * socket = fdopen(sockfd, "r");

	char * buf;
	size_t bytesRead = 0;
	int totalBytesRead = 0;
    
	while (getline(&buf, &bytesRead, socket) > 0) {
		strncat(response->response, buf, bytesRead - 1);
		totalBytesRead += bytesRead;

		if (buf[3] == ' ') {
			sscanf(buf, "%s", response->code);
			break;  
		}
    }

	free(buf);

	printf("< %s", response->response);

    return totalBytesRead;
}

int write_command(int sockfd, char *header, char *body, int reading_file) {

    if (write_to_socket(sockfd, 1, header, body) < 0) {
        printf("Error Sending Command  %s %s\n", header, body);
        return -1;
    }

    struct socket_response response;

    while (1) {

        read_response(sockfd, &response);

        switch (response.code[0]) { //switch with the different codes that can be given by the server
            case '1':
                // Expecting another reply
                if (reading_file) return 2;
                else break;
            case '2':
                // Success
                return 2;
            case '3':
                // Needs additional information (such as the password)
                return 3;
            case '4':
                // Error, try again
                if (write_to_socket(sockfd, 1, header, body) < 0) {
                    printf("Error Sending Command  %s %s\n", header, body);
                    return -1;
                }
                break;
            case '5':
                // Error in sending command, closing control socket, exiting application
                printf("Command wasn't accepted... \n");
                close(sockfd);
                exit(-1);
                break;
            default:
                break;
        }
    }

    return 0;
}

int login(struct url_args * parsed_args, int sockfd) {
    printf("\nSending Username...\n");
    int rtr = write_command(sockfd, "user", parsed_args->user, 0);
    if (rtr == -1) {
        printf("Error sending Username...\n\n");
        return -1;
    }
    printf("\nSending Password...\n");
    rtr = write_command(sockfd, "pass", parsed_args->password, 0);
    if (rtr == -1) {
        printf("Error sending Password...\n\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
        
    if (argc != 2) {
        printf("Usage: rc ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    struct url_args parsed_args;
    parse_input_url(argv[1], &parsed_args);
    
    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
    size_t bytes;

    struct socket_response response;
    
    /*send a string to the server*/
    int sockfd = create_connect_socket(parsed_args.ip);
    read_response(sockfd, &response);
    if (login(&parsed_args, sockfd) != 0) {
        perror("Error in login");
        exit(-1);
    } 
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

/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */
 
#include "clientTCP.h"

void get_filename(char * path, struct url_args * parsed_args) {
    char * aux = "";
    char directories[1024];
    char * token = strtok(path, "/");
   
    while(token != NULL) {
        if(strcmp(aux,"")){
            strcat(directories, aux);
            strcat(directories, "/");
        }
        aux = token;
        token = strtok(NULL, "/");
    }

    strcpy(parsed_args->path, directories);
    strcpy(parsed_args->filename, aux);
}

int parse_input_url(char * url, struct url_args * parsed_args) {

    char * ftp = strtok(url, ":");
    char * args = strtok(NULL, "/");
    char * path = strtok(NULL, "");

    if (ftp == NULL || args == NULL || path == NULL) {
        printf("Couldnt process given URL\n");
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
    else {
        strcpy(parsed_args->user, "anonymous");
        strcpy(parsed_args->password, "pass");
        strcpy(parsed_args->host, args);
    }

    get_filename(path, parsed_args);

    if (!strcmp(parsed_args->host, "") || !strcmp(parsed_args->path, "")) {
        printf("Couldnt process given URL\n");
        return -1;
    }

    struct hostent * h;

    if ((h = gethostbyname(parsed_args->host)) == NULL) {  
        printf("Error in gethostbyname\n");
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

int create_connect_socket(char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;
    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error in call to function socket()\n");
        exit(-1);
    }

    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        printf("Error in call to function connect()\n");
        exit(-1);
    }

    return sockfd;
}

int write_to_socket(int sockfd, int hasBody, char *header, char *body) {
    char message[512];
	strcpy(message, header);

	if (hasBody) {
		strcat(message, " ");
		strcat(message, body);
	}

	strcat(message, "\r\n");

    int bytesSent = write(sockfd, message, strlen(message));
    if (bytesSent != strlen(message)) {		
        printf("Error writing message to socket!\n");
        return -1;
    }

	printf("> %s", message);

    return bytesSent;
}

int read_response(int sockfd, struct socket_response * response) {
    strcpy(response->response, "");

	FILE * socket = fdopen(sockfd, "r");

	char * buf;
	size_t n_bytes = 0;
	int bytes_read = 0;
    
	while (getline(&buf, &n_bytes, socket) > 0) {
		strncat(response->response, buf, n_bytes - 1);
		bytes_read += n_bytes;

		if (buf[3] == ' ') {
			sscanf(buf, "%s", response->code);
			break;  
		}
    }

	free(buf);

	printf("< %s", response->response);

    return bytes_read;
}

int write_command(int sockfd, char* header, char* body, int reading_file, struct socket_response* response) {

    if (write_to_socket(sockfd, 1, header, body) < 0) {
        printf("Error Sending Command  %s %s\n", header, body);
        return -1;
    }

    while (1) {

        if(read_response(sockfd, response) == 0){
            printf("Error getting response from server\n");
            return -1;
        }

        switch (response->code[0]) {
            case '1':
                if (reading_file) return 2;
                else break;
            case '2':
                // Success
                return 2;
            case '3':
                // Missing input
                return 3;
            case '4':
                // Error
                if (write_to_socket(sockfd, 1, header, body) < 0) {
                    printf("Error Sending Command  %s %s\n", header, body);
                    return -1;
                }
                break;
            case '5':
                // Error in sending command
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
    struct socket_response response;
    if (write_command(sockfd, "user", parsed_args->user, 0, &response) == -1) {
        printf("Error sending username...\n\n");
        return -1;
    }
    if (write_command(sockfd, "pass", parsed_args->password, 0, &response) == -1) {
        printf("Error sending password\n\n");
        return -1;
    }
    return 0;
}

int passive_mode(int sockfd) {
    struct socket_response response;
    int data_sockfd;
    int r = write_command(sockfd, "pasv", "", 0, &response);
    if (r == -1) {
        printf("Error sending passive mode\n\n");
        return -1;
    }
    else if (r == 2) {
        int ip_1, ip_2, ip_3, ip_4;
        int port_1, port_2;
        if ((sscanf(response.response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).",
                    &ip_1, &ip_2, &ip_3, &ip_4, &port_1, &port_2)) < 0) 
        {
            printf("Error in processing information to calculate port\n\n");
            return -1;
        }   
        char ip[MAX_IP_LENGTH];
        sprintf(ip, "%d.%d.%d.%d", ip_1, ip_2, ip_3, ip_4);
        int port = port_1 * 256 + port_2;
        printf("Port number %d\n", port);
        if ((data_sockfd = create_connect_socket(ip, port)) < 0) {
            printf("Error creating new socket\n\n");
            return -1;
        }
    }
    else {
        printf("Error receiving pasv command response from server...\n\n");
        return -1;
    }
    return data_sockfd;
}

int download_file(int sockfd, int data_sockfd, struct url_args * parsed_args) {
    struct socket_response response;
    char path[1024];
    strcpy(path, parsed_args->path);
    strcat(path, parsed_args->filename);

    int r = write_command(sockfd, "retr", path, 1, &response);
    if (r == -1) {
        printf("Error sending file command\n\n");
        return -1;
    }
    else if (r == 2) {
        FILE *fp = fopen(parsed_args->filename, "w");
        if (fp == NULL){
            printf("Error opening or creating file\n\n");
            return -1;
        }
        char data[2048];
        int n_bytes;
        printf("\nStarting to download %s\n", parsed_args->filename);
        while((n_bytes = read(data_sockfd, data, sizeof(data)))){
            if(n_bytes < 0){
                printf("Error reading from socket\n\n");
                return -1;
            }
            if((n_bytes = fwrite(data, n_bytes, 1, fp)) < 0){
                printf("Error writing to file\n\n");
                return -1;
            }
        }
        printf("\nFinished downloading %s\n\n", parsed_args->filename);
        if(fclose(fp) < 0){
            printf("Error closing file\n\n");
            return -1;
        }
        close(data_sockfd);
        if(read_response(sockfd, &response) == 0){
            printf("Error in read_response after downloading\n");
            exit(-1);
        };
        if (response.response[0] != '2')
            return -1;
        return 0;
    }
    else {
        printf("Error receiving file command response from server...\n\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
        
    if (argc != 2) {
        printf("Usage: download ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    struct url_args parsed_args;

    if(parse_input_url(argv[1], &parsed_args) != 0){
        return -1;
    };

    struct socket_response response;
    
    int sockfd = create_connect_socket(parsed_args.ip, SERVER_PORT);
    if (read_response(sockfd, &response) == 0) {
        printf("Error in read_response after connecting to socket\n");
        exit(-1);
    };
    if (login(&parsed_args, sockfd) != 0) {
        printf("Error in login\n");
        exit(-1);
    } 
    int data_sockfd;
    if ((data_sockfd = passive_mode(sockfd)) == -1) {
        printf("Error in creating data socket\n");
        exit(-1);
    }
    if (download_file(sockfd, data_sockfd, &parsed_args) != 0) {
        printf("Error in download file function\n");
        exit(-1);
    }
    if (close(sockfd)<0) {
        printf("Error while closing socket\n");
        exit(-1);
    }
    return 0;
}

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

        read_response(sockfd, response);

        switch (response->code[0]) { //switch with the different codes that can be given by the server
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

int update_working_directory(int sockfd, char * path) {
    struct socket_response response;
    if(write_command(sockfd, "CWD", path, 0, &response) < 0){
        printf("Error sending command CWD\n\n");
        return -1;
    }
	return 0;
}

int download_file(int sockfd, int data_sockfd, struct url_args * parsed_args) {
    struct socket_response response;
    /*char path[1024];
    strcpy(path, parsed_args->path);
    strcat(path, "/");
    strcat(path, parsed_args->filename);*/

    int r = write_command(sockfd, "retr", parsed_args->filename, 1, &response);
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
        printf("Starting to download %s\n", parsed_args->filename);
        while((n_bytes = read(data_sockfd, data, sizeof(data)))){
            if(n_bytes < 0){
                printf("Error reading from socket\n\n");
                return -1;
            }
            printf("%s", data);
            if((n_bytes = fwrite(data, n_bytes, 1, fp)) < 0){
                printf("Error writing to file\n\n");
                return -1;
            }
        }
        printf("\nFinished downloading %s\n", parsed_args->filename);
        if(fclose(fp) < 0){
            printf("Error closing file\n\n");
            return -1;
        }
        close(data_sockfd);
        read_response(sockfd, &response);
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
        printf("Usage: rc ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    struct url_args parsed_args;

    parse_input_url(argv[1], &parsed_args);

    struct socket_response response;
    
    /*send a string to the server*/
    int sockfd = create_connect_socket(parsed_args.ip, SERVER_PORT);
    read_response(sockfd, &response);
    if (login(&parsed_args, sockfd) != 0) {
        perror("Error in login");
        exit(-1);
    } 
    int data_sockfd;
    if ((data_sockfd = passive_mode(sockfd)) == -1) {
        perror("Error in passive mode");
        exit(-1);
    }
    if (strlen(parsed_args.path) > 0) {
        if (update_working_directory(sockfd, parsed_args.path) < 0)
        {
            printf("Error changing directory\n\n");
            return -1;
        }
    }
    if (download_file(sockfd, data_sockfd, &parsed_args) != 0) {
        perror("Error in passive mode\n\n");
        exit(-1);
    }
    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }
    return 0;
}

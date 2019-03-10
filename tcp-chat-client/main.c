#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdbool.h>
#include <pthread.h>

#define MAXBUFLEN 32
#define MAXDATASIZE MESSAGESIZE + COMMANDSIZE + USERNAMESIZE
#define COMMANDSIZE 1
#define USERNAMESIZE 20
#define MESSAGESIZE 1000
#define N 5

#define UNICAST 1
#define BROADCAST 2
#define LIST 3
#define EXIT 4
#define JOIN 5
#define ERROR 6


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);
int join(int sockfd, char* username);
void listenForTyping(int sockfd);
void *listenForMessages(int *sock);


struct instruction {
    int command;
    char username[USERNAMESIZE];
    char message[MESSAGESIZE];
};

int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    pthread_t thread;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char* server_address, server_port, username;
            
    char buf[MAXDATASIZE];

    
    char s[INET6_ADDRSTRLEN];
    
    /*
    if (argc != 4) {
        fprintf(stderr,"usage: tcp-chat-client [server_address] [server_port] [user_name]\n");
        exit(1);
    }
     */
    
    server_address = argv[1];
    server_port = argv[2];
    username = argv[3];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    /*
    if ((rv = getaddrinfo(server_address, server_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
     */
    
    if ((rv = getaddrinfo(NULL, "3490", &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connected to chat server at %s\n", s);
    
    
    join(sockfd, "ben"); //join chat
    
    pthread_create(&thread, NULL, (void*) listenForMessages, (void*) &sockfd); //use thread to allow listening and typing concurrently
    
    listenForTyping(sockfd);

    pthread_join(thread, NULL);
    
    freeaddrinfo(servinfo);

    close(sockfd);

    return 0;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//requests to join chat group
//returns 1 if successful
//return -1 if unsuccessful
int join(int sockfd, char* username){
    char sendbuf[MAXDATASIZE];
    char recvbuf[MAXDATASIZE];
    
    struct instruction recvinst;
    struct instruction inst;
    
    memset(&inst, 0, MAXDATASIZE);
    memset(sendbuf, 0, MAXDATASIZE);
    memset(recvbuf, 0, MAXDATASIZE);
    
 
    inst.command = JOIN;
    memcpy(inst.username, username, USERNAMESIZE);
    memcpy(sendbuf, &inst, MAXDATASIZE);
    
    
    if (send(sockfd, sendbuf, MAXDATASIZE, 0) == -1){
        perror("client: send: ");
        exit(1);
    }

    //wait for confirmation that you've joined
    if ((recv(sockfd, recvbuf, MAXDATASIZE, 0)) == -1) {
        perror("client: recv: ");
        exit(1);
    }
    
    recvinst = *((struct instruction *)recvbuf);
    if (recvinst.command == ERROR){
        printf("Error joining: %s\n", recvinst.message);
        return -1;
    }
    else{
        printf("%s\n", recvinst.message);
        return 1;
    }
}


//function that calls recv to check for messages relayed by server
//responds accordingly depending on type of command received
void *listenForMessages(int *sock){
    char recvbuf[MAXDATASIZE];
    struct instruction recvinst;
    bool connected = true;
    int numbytes;
    int sockfd = *sock;
     
    while(connected){
        if ((numbytes = recv(sockfd, recvbuf, MAXDATASIZE, 0)) == -1){ 
            perror("client: recv");
            exit(1);
        }
        if (numbytes > 0){
            printf("numbytes: %d\n", numbytes);
            recvinst = *((struct instruction *)recvbuf);

            if(recvinst.command == UNICAST || recvinst.command == BROADCAST){
                printf("%s: ", recvinst.username);
                printf("%s\n", recvinst.message);
            }

            if(recvinst.command == EXIT){
                printf("Exited chat\n");
                connected = false;
            }

            if(recvinst.command == LIST){
                printf("List of clients: %s\n", recvinst.message);
                connected = false;
            }
            
            if(recvinst.command == ERROR){
                printf("Error message: %s\n", recvinst.message);
            }

            memset(recvbuf, 0, MAXDATASIZE);
        }
    
    }
}

//continuously checks for user input
//trasnmits packets to server depending on user input
void listenForTyping(int sockfd){
    
    bool exited = false;
    char string[MAXDATASIZE];
    char command[20], message[MESSAGESIZE];
    struct instruction inst;
    char sendbuf[MAXDATASIZE];
    char recvbuf[MAXDATASIZE];
    char * broad = "broadcast";
    
    memset(command, 0, COMMANDSIZE);
    memset(message, 0, MESSAGESIZE);
    memset(recvbuf, 0, MAXDATASIZE);
    
    while (!exited){
        scanf(" %[^\n]s", string);
        char* place = strchr(string, ' ');
        
        if (place == NULL){
            //one word command
            if(strcmp(string, "exit") == 0){
                exited = true;
                inst.command = EXIT;
                memset(inst.username, 0, USERNAMESIZE);
                memset(inst.message, 0, MESSAGESIZE);
            
            }
            else if(strcmp(string, "list") == 0){
                inst.command = LIST;
                memset(inst.username, 0, USERNAMESIZE);
                memset(inst.message, 0, MESSAGESIZE);
            }
            else{
                printf("Command not recognized\n");
                continue;
            }
        }
        else{
            //two word command
            //break it up into two strings
            for(int i = 0; i < strlen(string); i++){
                if (string[i] == ' '){
                    memcpy(command, string, i);
                    command[i] = '\0';
                    memcpy(message, string+i+1, strlen(string)-(i+1));
                    message[strlen(string)-(i+1)] = '\0';
                    break;
                }
                
            }
            
            if (strcmp(command,broad) == 0){
                inst.command = BROADCAST;
                memset(inst.username, 0, USERNAMESIZE);
            }
            else{
                inst.command = UNICAST;
                memcpy(inst.username, command, USERNAMESIZE);
            }
            
            memcpy(inst.message, message, strlen(message));
             
        }
            memcpy(sendbuf, &inst, MAXDATASIZE);
            if (send(sockfd, sendbuf, MAXDATASIZE, 0) == -1){
                        perror("client: send");
                        exit(1);
            }
        
            memset(sendbuf, 0, MAXDATASIZE);
            
    }
    
}




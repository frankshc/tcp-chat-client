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
#define MAXDATASIZE 200 
#define N 5

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);
//char* concat(const char *s1, const char *s2);
//char* pad(const char *s);
void listenForTyping(int sockfd);
void *listenForMessages(int sockfd);

struct instruction {
    char command[20];
    char username[20];
    char message[100];
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
    
    //must first try to join server, send JOIN [USERNAME] to the server
    //server will tell me if it succeeded, otherwise i will have to try again with a diff username
    
    join(sockfd, "ben");
    
    pthread_create(&thread, NULL, listenForMessages, sockfd);
    
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


//return -1 if unsuccessful
int join(int sockfd, char* username){
    char sendbuf[MAXDATASIZE];
    char recvbuf[MAXDATASIZE];
    
    struct instruction recvinst;
    
    memset(sendbuf, 0, MAXDATASIZE);
    memset(recvbuf, 0, MAXDATASIZE);
    char *command = "JOIN"; 
    
    struct instruction inst;
    memset(&inst, 0, 140);

    
    memcpy(inst.command, command, strlen(command));
    memcpy(inst.username, username, strlen(username));

    memcpy(sendbuf, &inst, 140);
    
    
    if (send(sockfd, sendbuf, MAXDATASIZE, 0) == -1){
        perror("client: sendto");
        exit(1);
    }
    
    //wait for confirmation that you've joined
    if ((recv(sockfd, recvbuf, MAXDATASIZE-1, 0)) == -1) {
        perror("client: recv");
        exit(1);
    }
    
    recvinst = *((struct instruction *)recvbuf);
    
    if (strcmp(recvinst.message, "ERROR") == 0)
        return -1;
    else{
        printf("%s\n", recvinst.message);
        return 1;
    }
}



void *listenForMessages(int sockfd){
    char recvbuf[MAXDATASIZE];
    struct instruction recvinst;
    bool connected = true;
     
    while(connected){
        if ((recv(sockfd, recvbuf, MAXDATASIZE-1, 0)) == -1){ 
            perror("client: recv");
            exit(1);
        }
        recvinst = *((struct instruction *)recvbuf);
        
        printf("Command: %s \n", recvinst.command);
        printf("Username: %s \n", recvinst.username);
        printf("Message: %s \n", recvinst.message);
            
        
        if(strcmp(recvinst.command, "MESSAGE") == 0){
            printf("Received message from: %s\n", recvinst.username);
            printf("Message: %s\n", recvinst.message);
        }
        
        if(strcmp(recvinst.command, "EXIT") == 0){
            printf("Exited chat\n");
            connected = false;
        }
        
        if(strcmp(recvinst.command, "LIST") == 0){
            printf("List of clients: %s\n", recvinst.message);
            connected = false;
        }
        
    
    }
}

void listenForTyping(int sockfd){
    
    bool exited = false;
    char string[MAXDATASIZE];
    char command[20], message[100];
    struct instruction inst, recvinst;
    char sendbuf[MAXDATASIZE];
    char recvbuf[MAXDATASIZE];
    
    memset(command, 0, 20);
    memset(message, 0, 100);
    memset(recvbuf, 0, MAXDATASIZE);
    
    while (!exited){
        printf("Type: ");
        scanf(" %[^\n]s", string);
        char* place = strchr(string, ' ');
        
        if (place == NULL){
            if(strcmp(string, "exit") == 0){
                exited = true;
            
            }
            else if(strcmp(string, "list") == 0){
            
            
            }
            else{
                printf("Command not recognized\n");
            }
        }
        else{
            for(int i = 0; i < strlen(string); i++){
                if (string[i] == ' '){
                    memcpy(command, string, i);
                    command[i] = '\0';
                    memcpy(message, string+i+1, strlen(string)-(i+1));
                    message[strlen(string)-(i+1)] = '\0';
                    break;
                }
                
            }
            memcpy(inst.command, command, strlen(command));
            memcpy(inst.message, message, strlen(message));
            memset(inst.username, 0, 20);
            memcpy(sendbuf, &inst, 140);

            
             if (send(sockfd, sendbuf, MAXDATASIZE, 0) == -1){
                perror("client: send");
                exit(1);
            }
    
  
        }
            
    }
    
}




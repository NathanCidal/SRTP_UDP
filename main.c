#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "srtp.h"

#define PORT 5000
#define MAXLINE 1000

void interface_listen(){
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    int listenfd, len;
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);        
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET; 
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));  
    len = sizeof(cliaddr);

    uint8_t window_size_stabilished = 39;
    uint8_t window_size_client = 255;

    #ifdef DEBUG
    printf("> Comecando o SRTP_Listen!\n");
    #endif

    window_size_client = srtp_listen(listenfd, &servaddr);
    if(window_size_client < window_size_stabilished) window_size_stabilished = window_size_client;

    #ifdef DEBUG
    printf("> Janela estabelecida: %d\n", window_size_stabilished);
    printf("> SRTP Listen Concluido!\n");
    printf("> Comecando o SRTP Accept\n");
    #endif

    int verify = 0;
    verify = srtp_accept(listenfd, &servaddr, window_size_stabilished);

    #ifdef DEBUG
    printf("> SRTP Accept Concluido!\n");
    #endif

    if(!verify){
        printf("Error!\n");
        close(listenfd);
    }

    close(listenfd);
}

void interface_host(){
    int sockfd;
    struct sockaddr_in servaddr;
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    // Stabilish Connection to the Server IP
    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("\n Error : Connect Failed \n");
        exit(0);
    }

    uint8_t window_size_desired = 35;
    uint8_t window_size_stablished = 55;

    #ifdef DEBUG
    printf("> Comecando o Connect!\n");
    #endif
    
    window_size_stablished = srtp_connect(sockfd, &servaddr, window_size_desired);

    #ifdef DEBUG
    printf("> Janela estabelecida: %d\n", window_size_stablished);
    #endif
    
    // End of Communication
    close(sockfd);
}

int main(int argc, char * argv[]){
        // --listen -> Cant be host
        // --port -> Default, must be used for both, i guess?
        // --file (only on sender, its half-duplex)
        // --host (ip) != listenner

        if(argc >= 2){  
            if(strcmp(argv[1], "--host") == 0){
                printf("Host!\n");
                interface_host();
            }
            if(strcmp(argv[1], "--listen") == 0){
                printf("Listen!\n");
                interface_listen();
            }
        }

        return 0;
}
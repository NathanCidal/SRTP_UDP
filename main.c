#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "parser.h"
#include "srtp.h"

int interface_listen(uint16_t port_in, uint8_t protocol_mode, uint16_t window_size){
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    int listenfd, len;
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);        
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port_in);
    servaddr.sin_family = AF_INET; 
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));  
    len = sizeof(cliaddr);

    uint8_t window_size_stabilished = window_size;
    uint8_t window_size_client = 255;

    #ifdef DEBUG
    printf("> Begin - SRTP_Listen!\n");
    #endif

    window_size_client = srtp_listen(listenfd, &servaddr);
    if(window_size_client < window_size_stabilished) window_size_stabilished = window_size_client;

    #ifdef DEBUG
    printf("> Stabilished Window: %d\n", window_size_stabilished);
    printf("> Begin - SRTP_Accept\n");
    #endif

    int verify = 0;
    verify = srtp_accept(listenfd, &servaddr, window_size_stabilished);

    #ifdef DEBUG
    printf("> End - SRTP_Accept!\n");
    #endif

    if(!verify){
        printf("Error!\n");
        close(listenfd);
        return 1;
    }

    int sockfd_data = listenfd;

    FILE * fp = fopen("output_file.txt", "w+");
    srtp_receive(sockfd_data, port_in, fp, &servaddr, window_size_stabilished, protocol_mode);
    close(sockfd_data);
    return 0;
}

int interface_host(uint16_t port_in, uint8_t protocol_mode, char * ip, char * file_name, uint16_t window_size){
    int sockfd;
    struct sockaddr_in servaddr;
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port_in); // Destino é a porta P (6000)
    servaddr.sin_family = AF_INET;
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in local_src_addr;
    bzero(&local_src_addr, sizeof(local_src_addr));
    local_src_addr.sin_family = AF_INET;
    local_src_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    local_src_addr.sin_port = htons(port_in + 1);       

    if (bind(sockfd, (struct sockaddr*)&local_src_addr, sizeof(local_src_addr)) < 0) {
        perror("! Erro ! Falha ao fixar a porta de origem P+1 no Host");
        close(sockfd);
        return 1;
    }

    struct timeval time_value;
    time_value.tv_sec = 0;
    time_value.tv_usec = 100000; 
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &time_value, sizeof(time_value));

    #ifdef DEBUG
    printf("> Begin SRTP_Connect!\n");
    #endif
    
    uint8_t window_size_stablished = srtp_connect(sockfd, &servaddr, window_size);

    #ifdef DEBUG
    printf("> Stabilished Window: %d\n", window_size_stablished);
    #endif

    int sockfd_data = sockfd;
    int sockfd_ack  = sockfd; 

    FILE * fp = fopen(file_name, "r");
    if(!fp){
        printf("! Error ! - Input File Missing\n");
        srtp_close(sockfd_data, sockfd_ack, &servaddr, 0);
        close(sockfd);
        return 1;
    }

    uint16_t count = srtp_send(sockfd_data, sockfd_ack, fp, &servaddr, window_size_stablished, protocol_mode);
    fclose(fp);

    // Finalizes Communication
    srtp_close(sockfd_data, sockfd_ack, &servaddr, count);
    close(sockfd);
    return 0;
}

int main(int argc, char * argv[]){
        uint8_t error_detected = 0;
        uint8_t * parameters = parser_worker(argc, argv);
        error_detected = parser_error_detector(parameters);

        if(error_detected){
            free(parameters);
            return 1;
        }

        // Decides the Operation Mode
        uint8_t execution_mode = 1; // Stop and Wait (By Default)
        if(parameters[MODE_P]){
            if(parameters[SAW_P]) execution_mode = 1;
            if(parameters[GBN_P]) execution_mode = 2;
            if(parameters[SR_P])  execution_mode = 3;
        }

        uint8_t listen_mode = 0;
        uint8_t host_mode   = 0;
        if(parameters[LISTEN_P]) listen_mode = 1;
        if(parameters[HOST_P]) host_mode = 1;

        uint16_t port_value = 0;
        if(parameters[PORT_VAL_P]) port_value = (uint16_t)atoi(argv[parameters[PORT_VAL_P]]);

        char ip_port[BUFFER_SIZE];
        if(parameters[IP_P]) strcpy(ip_port, argv[parameters[IP_P]]);

        char file_name[BUFFER_SIZE];
        if(parameters[FILE_NAME_P]) strcpy(file_name, argv[parameters[FILE_NAME_P]]);

        uint16_t window_size = 1;
        if(parameters[SIZE_VAL_P]) window_size = atoi(argv[parameters[SIZE_VAL_P]]);
        if(window_size > 0x3FFF) window_size = 0x3FFF;
        if(window_size == 0) window_size = 1;

        free(parameters);

        if(host_mode)
        {
            printf("> Host Application!\n");
            interface_host(port_value, execution_mode, ip_port, file_name, window_size);
        }
        else if(listen_mode)
        {
            printf("> Listen Application!\n");
            interface_listen(port_value, execution_mode, window_size);
        }

        return 0;
}
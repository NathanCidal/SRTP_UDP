#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parser.h"
#include "srtp.h"

int interface_listen(uint16_t port_in, uint8_t protocol_mode){
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    int listenfd, len;
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);        
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port_in);
    servaddr.sin_family = AF_INET; 
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));  
    len = sizeof(cliaddr);

    uint8_t window_size_stabilished = 39;
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
    }


    int sockfd_data = listenfd;

    FILE * fp = fopen("output_file.txt", "w+");
    srtp_receive(sockfd_data, port_in, fp, &servaddr, window_size_stabilished, 0);
    close(sockfd_data);
    return 0;
}

int interface_host(uint16_t port_in, uint8_t protocol_mode, char * ip, char * file_name){
    int sockfd;
    struct sockaddr_in servaddr;
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(port_in);
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
    printf("> Begin SRTP_Connect!\n");
    #endif
    
    window_size_stablished = srtp_connect(sockfd, &servaddr, window_size_desired);

    #ifdef DEBUG
    printf("> Stabilished Window: %d\n", window_size_stablished);
    #endif


    // Communication Process with Dual-Port
    int sockfd_data = sockfd;
    int sockfd_ack = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in local_ack_addr;
    bzero(&local_ack_addr, sizeof(local_ack_addr));
    local_ack_addr.sin_family = AF_INET;
    local_ack_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    local_ack_addr.sin_port = htons(port_in + 1);       

    if (bind(sockfd_ack, (struct sockaddr*)&local_ack_addr, sizeof(local_ack_addr)) < 0) {
        perror("! Erro ! Binding Error on P+1 on Sender");
        close(sockfd);
        close(sockfd_ack);
        return 1;
    }

    // Configurate Socket to have support for Timeouts
    struct timeval time_value;
    time_value.tv_sec = 0;
    time_value.tv_usec = 100000; //100ms - Fixed time in the specification
    setsockopt(sockfd_ack, SOL_SOCKET, SO_RCVTIMEO, &time_value, sizeof(time_value));

    FILE * fp = fopen(file_name, "r");
    if(!fp){
        printf("! Error ! - Input File Missing\n");
        srtp_close(sockfd_data, sockfd_ack, &servaddr, 0);
        close(sockfd_ack);
        close(sockfd_data);
        return 1;
    }

    uint16_t count = srtp_send(sockfd_data, sockfd_ack, fp, &servaddr, window_size_stablished, 0);
    fclose(fp);

    // Finalizes Communication
    srtp_close(sockfd_data, sockfd_ack, &servaddr, count);
    close(sockfd);
    close(sockfd_ack);
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

        free(parameters);

        if(host_mode)
        {
            printf("> Host Application!\n");
            interface_host(port_value, execution_mode, ip_port, file_name);
        }
        else if(listen_mode)
        {
            printf("> Listen Application!\n");
            interface_listen(port_value, execution_mode);
        }

        return 0;
}
#ifndef _SRTP_H
#define _SRTP_H

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <sys/time.h>

#define BUFFER_SIZE 4096
#define MAX_TRIES    100

#define STOP_AND_WAIT_MODE 1
#define GO_BACK_N_MODE     2
#define SELETIVE_REPEAT    3

struct srtp_header_t{
    uint8_t  syn;
    uint8_t  fin;
    uint16_t seq;
    uint8_t  ack_flag;
    uint8_t  nack;
    uint16_t ack_num;
    uint8_t  length;
    uint32_t crc32;
};

// Three-Way Handshake
int srtp_listen(int sockfd, struct sockaddr_in *client_addr);
int srtp_connect(int sockfd, struct sockaddr_in *server_addr, uint8_t window_size);
int srtp_accept(int sockfd, struct sockaddr_in *client_addr, uint8_t window_size);

// Process of communication

int srtp_send_gbn(int sockfd_data, int sockfd_ack, FILE *file, const struct sockaddr_in *dest_addr, uint8_t window_size);
int srtp_send_saw(int sockfd_data, int sockfd_ack, FILE *file, const struct sockaddr_in *dest_addr, uint8_t window_size);
int srtp_send(int sockfd_data, int sockfd_ack, FILE *file, const struct sockaddr_in *dest_addr, uint8_t window_size, int mode);





int srtp_receive(int sockfd_data, uint16_t port_in, FILE * file_output, struct sockaddr_in * source_addr, uint8_t window_size, int mode);

// Communication Finish Handshake
int srtp_close(int sockfd_data, int sockfd_ack, struct sockaddr_in *dest_addr, uint16_t last_seq_count);

#endif
// srtp.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "zlib.h"

#include "srtp.h" // Inclui o seu header para o compilador checar as assinaturas

/*
 * Based on the header data and the payload, generates the data to be send
 * Returns pointer to the data (must free after)
 */
uint8_t * srtp_data(struct srtp_header_t s, uint8_t * payload){
        uint8_t * data;
        uint8_t payload_size = 0;
        if(!payload){ 
                data = malloc(sizeof(uint8_t) * 9); 
        }
        else{
                data = malloc(sizeof(uint8_t) * (9 + s.length)); payload_size = s.length;
        }
        
        data[0]  = (s.syn) << 7;
        data[0] |= (s.fin) << 6;
        data[0] |= (uint8_t)((s.seq >> 8) & 0x3F);
        data[1]  = (uint8_t)(s.seq & 0xFF);
        data[2]  = (s.ack_flag) << 7;
        data[2] |= (s.nack) << 6;
        data[2] |= (uint8_t)((s.ack_num >> 8) & 0x3F);
        data[3]  = (uint8_t)(s.ack_num & 0xFF);
        data[4]  = s.length;
        for(int i = 5; i < 9; i++){
                data[i] = 0;
        }
        for(int i = 0; i < payload_size; i++){
                data[i+9] = payload[i];
        }

        uint32_t crc32_value = (uint32_t)crc32(0, data, 9 + payload_size);
        data[5] = (crc32_value >> 24) & 0xFF;
        data[6] = (crc32_value >> 16) & 0xFF;
        data[7] = (crc32_value >>  8) & 0xFF;
        data[8] = (crc32_value >>  0) & 0xFF;
        return data;
}


/*
 * Verifies the Checksum and return '1' on Correct, '0' in case of incorrect CRC32
 */
int srtp_checksum(uint8_t * data, int size){
        // Incorrect Size for Communication
        if(size < 9){
                return 0;
        }

        uint8_t aux[4];
        for(int i = 0; i < 4; i++){
            aux[i] = data[i+5];
            data[i+5] = 0;
        }

        uint32_t checksum_calculated = (uint32_t)crc32(0, data, size);

        for(int i = 0; i < 4; i++){
            data[i+5] = aux[i];
        }

        // Converter para Little-Endian
        uint32_t checksum_received =
            ((uint32_t)data[5] << 24) |
            ((uint32_t)data[6] << 16) |
            ((uint32_t)data[7] <<  8) |
            ((uint32_t)data[8] <<  0);    

        return (checksum_received == checksum_calculated);
}

/*
 * SRTP_Listen
 *      - Used to receive connection fron outside
 *      - Returns to the user the Window_Size
 *      - 0    - Error
 *      - Else - Correct + Window sent (Window_size must be treated after this step of communication)
 */
int srtp_listen(int sockfd, struct sockaddr_in *client_addr){
        uint8_t buffer[BUFFER_SIZE];
        uint8_t pass = 0;
        int len = sizeof(struct sockaddr_in);
        while(1){
                int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct  sockaddr*)client_addr, &len);
                pass = (srtp_checksum(buffer, n));

                // Somente para Loop se receber uma mensagem de SYN
                if(pass && (buffer[0] & 0x80)) break;        
        }
        uint8_t client_window = buffer[4];
        return client_window;
}       

/*
 * SRTP_Connect
 *      - Stabilishes connection with Listenner
 *      - Returns the window_size negotiated
 *      0 - Error
 *      Else - Correct and the window_size (API takes care of window_size for Sender/Host)
 */
int srtp_connect(int sockfd, struct sockaddr_in *server_addr, uint8_t window_size){
        uint8_t * connect_message;
        uint8_t window_size_client = window_size;
        uint8_t window_size_decided;

        int len = sizeof(struct sockaddr_in);

        struct srtp_header_t connect_header = {
                .syn      = 1,
                .fin      = 0,
                .seq      = 0,
                .ack_flag = 0,
                .nack     = 0,
                .ack_num  = 0,
                .length   = window_size,
                .crc32    = 0
        };

        // Send SYN + Window_Size
        #ifdef DEBUG
        printf("> Enviando o SYN + WindowSize!\n");
        #endif
        
        connect_message = srtp_data(connect_header, 0);
        sendto(sockfd, connect_message, 9,  0, (struct sockaddr*)server_addr, len);
        free(connect_message);

        uint8_t buffer[BUFFER_SIZE];
        uint8_t pass = 0;

        // Waits for SYN+ACK from Receiver
        #ifdef DEBUG
        printf("> Esperando SYN+ACK!\n");
        #endif

        struct sockaddr_in from_addr; 

        while(1){
                int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct  sockaddr*)&from_addr, &len);
                pass = (srtp_checksum(buffer, n));

                // Somente para Loop se receber uma mensagem de SYN+ACK
                if(pass && ((buffer[0] & 0x80) && (buffer[2] & 0x80))) break;        
        }
        
        window_size_decided = buffer[4];
        if(window_size_decided > window_size_client)
        { 
                window_size_decided = window_size_client;
        }

        // Sends last ACK, stabilishing connection to the other side
        connect_header = (struct srtp_header_t) {
                .syn      = 0,
                .fin      = 0,
                .seq      = 0,
                .ack_flag = 1,
                .nack     = 0,
                .ack_num  = 0,
                .length   = 0,
                .crc32    = 0
        };

        #ifdef DEBUG
        printf("> Enviando o ACK!\n");
        #endif
        
        connect_message = srtp_data(connect_header, 0);
        sendto(sockfd, connect_message, 9,  0, (struct sockaddr*)server_addr, len);
        free(connect_message);

        return window_size_decided;
}

/*
 * Who makes treatment of window_size is not the SRPT API, must be made by the user outside of it
 * Returns 0 in case of timeout                 (Not Implemented)
 * Return 1 in case of correct connection
 */
int srtp_accept(int sockfd, struct sockaddr_in *client_addr, uint8_t window_size){
        uint8_t * accept_message;
        uint8_t window_size_listen = window_size;
        int len = sizeof(struct sockaddr_in);

        struct srtp_header_t connect_header = {
                .syn      = 1,
                .fin      = 0,
                .seq      = 0,
                .ack_flag = 1,
                .nack     = 0,
                .ack_num  = 0,
                .length   = window_size_listen,
                .crc32    = 0
        };

        // Sends SYN + Window_Size
        accept_message = srtp_data(connect_header, 0);
        sendto(sockfd, accept_message, 9,  0, (struct sockaddr*)client_addr, len);
        free(accept_message);

        struct sockaddr_in from_addr; 
        uint8_t buffer[BUFFER_SIZE];
        uint8_t pass = 0;

        // Waits for ACK from Sender
        while(1){
                int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct  sockaddr*)&from_addr, &len);
                pass = (srtp_checksum(buffer, n));

                // Somente para Loop se receber uma mensagem de SYN+ACK
                if(pass && (buffer[2] & 0x80)) break;        
        }

        return 1;
}

/*
 * API Function used to send FILE to another Host
 *
 * Modes:
 * 0 - SaW (Stop and Wait) - Uses at max 1 of Window Size
 * 1 - GBN (Go Back N)
 * 2 - SR  (Selective Repeat)
 */
int srtp_send(int sockfd, FILE *file, const struct sockaddr_in *dest_addr, uint8_t window_size, int mode){
        // First Breaks the FILE in 255 Bytes (for each send)
        uint8_t file_segmenent[255];
        size_t bytes_lidos = 0;
        uint16_t current_pos = 0;
        int len = sizeof(struct sockaddr_in);

        // Enquanto tem como segementar o arquivo em 255 Bytes, e segmentado.
        while((bytes_lidos = fread(file_segmenent, 1, 255, file)) > 0){
                struct srtp_header_t segement_header = {
                        .syn = 0,
                        .fin = 0,
                        .seq = current_pos,
                        .nack = 0,
                        .ack_num = 0,
                        .length = (uint8_t)bytes_lidos,
                        .crc32 = 0 
                };

                uint8_t * data_segmenet = srtp_data(segement_header, file_segmenent);
                sendto(sockfd, data_segmenet, 9 + segement_header.length, 0, (struct sockaddr*)dest_addr, len);
                free(data_segmenet);

                for(int i = 0; i < 255; i++){
                        printf("%c", file_segmenent[i]);
                }

                // Limpo o buffer de envio por garantia
                for(int i = 0; i < 255; i++){
                        file_segmenent[i] = 0;
                }
        }

        return 0;
}

/* 
 * API Function used to receive FILE from another Host (and to write in a output file)
 */
int srtp_receive(int sockfd, FILE * file_output, struct sockaddr_in * source_addr, uint8_t window_size, int mode){
        uint8_t receive_buffer[BUFFER_SIZE];
        uint16_t file_counter = 0;
        uint16_t seq_counter = 0;

        uint8_t * response_to_otherside;
        int n = 0;
        int check = 0;
        while(1){
                n = recvfrom(sockfd, receive_buffer, BUFFER_SIZE, 0, (struct sockaddr *)source_addr, sizeof(struct sockaddr_in));
                check = srtp_checksum(receive_buffer, n);
                
                if(check){
                        seq_counter = (receive_buffer[0] & 0x3F) << 8;
                        seq_counter |= receive_buffer[1]; 

                        // Correct Send Process
                        if((file_counter+1) == seq_counter){
                                for(int i = 9; i < receive_buffer[4]; i++){
                                        fprintf(file_output, "%c", receive_buffer[i]);
                                }
                                file_counter++;
                        }else{
                        // Incorrect ! Send NACK and Repeat process
                        break;
                        }
                }
        }
}

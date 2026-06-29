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
 *      - Used to receive connection fron outside (Infinite Loop waiting for a connection)
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
                if(n < 0) continue;
                pass = (srtp_checksum(buffer, n));

                // Somente para Loop se receber uma mensagem de SYN
                if(pass && (buffer[0] == 0x80) && (buffer[1] == 0x00) && (buffer[2] == 0x00) && (buffer[3] == 0x0)) break;        
        }
        uint8_t client_window = buffer[4];
        return client_window;
}       

/*
 * SRTP_Connect
 *      - Stabilishes connection with Listenner (Infinite Loop if never answered)
 *      - Returns the window_size negotiated when finish 
 */
int srtp_connect(int sockfd, struct sockaddr_in *server_addr, uint8_t window_size){
        uint8_t * connect_message;
        uint8_t window_size_client = window_size;
        uint8_t window_size_decided = 1;

        struct sockaddr_in from_addr; 
        int len = sizeof(struct sockaddr_in);

        // Monta o cabeçalho seguindo a estrutura de bits deles
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

        #ifdef DEBUG
        printf("> Sending SYN + WindowSize!\n");
        #endif

        uint8_t connected = 0;
        uint8_t buffer[BUFFER_SIZE];

        struct timeval time_value;
        time_value.tv_sec = 0;
        time_value.tv_usec = 100000; // 100ms - Fixed timeout
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &time_value, sizeof(time_value));

        while(!connected){
                connect_message = srtp_data(connect_header, 0);
                sendto(sockfd, connect_message, 9,  0, (struct sockaddr*)server_addr, len);
                free(connect_message);

                #ifdef DEBUG
                printf("> Waiting for SYN+ACK!\n");
                #endif

                int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&from_addr, &len);
                if(n < 0){
                        // Timeout: Sends SYN Again
                        continue;
                }

                if(srtp_checksum(buffer, n)){
                        // Checks SYN+ACK and others data segements must be zero
                        if (buffer[0] == 0x80 && buffer[1] == 0x00 &&
                            buffer[2] == 0x80 && buffer[3] == 0x00)
                        {
                            connected = 1;
                            window_size_decided = buffer[4];
                        }
                }        
        }
        
        if(window_size_decided > window_size_client) { 
                window_size_decided = window_size_client;
        }

        // Finish Handshake 
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
        printf("> Sending Handshake ACK!\n");
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

        uint8_t buffer[BUFFER_SIZE];
        uint8_t pass = 0;

        int current_tries = 0; // Parameter unused

        struct sockaddr_in from_addr; 


        // Waits for ACK from Sender
        while(1){
                // Sends SYN + Window_Size (In Loop waiting for connection)
                accept_message = srtp_data(connect_header, 0);
                sendto(sockfd, accept_message, 9,  0, (struct sockaddr*)client_addr, len);
                free(accept_message);
                
                int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct  sockaddr*)&from_addr, &len);
                if(n < 0) continue;
                pass = (srtp_checksum(buffer, n));

                // Only stops loop when receives final packet: 0x00 0x00 0x80 0x00 0x00 (CRC Ignores in this case)
                if(pass && (buffer[0] == 0x0) && (buffer[1] == (0x0)) && (buffer[2] == 0x80) && (buffer[3] == 0x0) && (buffer[4] == 0x0)){ 
                        break;
                }        
        }

        return 1;
}

/*
 * SRTP_Send in Seletive-Repeat Mode
 * Keeps a window of data (to send in case its needed)
 */
int srtp_send_sr(int sockfd_data, int sockfd_ack, FILE * file, const struct sockaddr_in *dest_addr, uint8_t window_size){
        uint8_t file_segments[255];
        int len = sizeof(struct sockaddr_in);
        int n = 0;
        int checksum_b = 0;

        uint8_t ** file_data = malloc(sizeof(uint8_t *) * window_size);
        for(int i = 0; i < window_size; i++){
                file_data[i] = malloc(sizeof(uint8_t) * 255);
        }

        for(int i = 0; i < window_size; i++){
                for(int j = 0; j < 255; j++){
                        file_data[i][j] = 0;
                }
        }

        uint8_t response[9];
        size_t bytes_readen = 0;
        uint16_t base = 0;
        uint16_t next_seq = 0;
        uint8_t  end_of_file = 0;
        uint16_t file_sizes[window_size];

        // SR Specific tracking structures
        uint8_t ack_flags[window_size];
        struct timeval timers[window_size];
        struct timeval now;
        for(int i = 0; i < window_size; i++){
                ack_flags[i] = 0;
                timers[i].tv_sec = 0;
                timers[i].tv_usec = 0;
        }

        uint16_t ack_count = 0;
        uint16_t send_ptr = 0;

        while(1){
                // Fills window based on available slots
                while (((uint16_t)(next_seq - base) & 0x3FFF) < window_size && !end_of_file) {
                        bytes_readen = fread(file_segments, 1, 255, file);
                        if (bytes_readen > 0) {
                                int buf_idx = next_seq % window_size;
                                file_sizes[buf_idx] = (uint16_t)bytes_readen;
                                ack_flags[buf_idx] = 0; // Unacknowledged
                                timers[buf_idx].tv_sec = 0; // Not transmitted yet
                                
                                for (int j = 0; j < bytes_readen; j++) {
                                        file_data[buf_idx][j] = file_segments[j];
                                }
                                next_seq = (next_seq + 1) & 0x3FFF;
                        } else {
                                end_of_file = 1;
                        }
                }

                // Transmission of new packets
                while (send_ptr != next_seq) {
                        int buf_idx = send_ptr % window_size;
                        
                        struct srtp_header_t segment_header = {
                                .syn = 0, .fin = 0,
                                .seq = send_ptr,
                                .ack_flag = 0, .nack = 0, .ack_num = 0,
                                .length = (uint8_t)file_sizes[buf_idx],
                                .crc32 = 0
                        };

                        uint8_t * data_segment = srtp_data(segment_header, file_data[buf_idx]);
                        sendto(sockfd_data, data_segment, 9 + segment_header.length, 0, (struct sockaddr*)dest_addr, len);
                        free(data_segment);

                        gettimeofday(&timers[buf_idx], NULL); // Start individual timer
                        send_ptr = (send_ptr + 1) & 0x3FFF;
                }

                // Check individual timeouts for packets currently in flight
                gettimeofday(&now, NULL);
                uint16_t check_ptr = base;
                while (check_ptr != next_seq) {
                        int buf_idx = check_ptr % window_size;
                        if (!ack_flags[buf_idx] && timers[buf_idx].tv_sec > 0) {
                                long elapsed = (now.tv_sec - timers[buf_idx].tv_sec) * 1000 + 
                                               (now.tv_usec - timers[buf_idx].tv_usec) / 1000;
                                
                                if (elapsed >= 100) { // Individual 100ms Timeout
                                        #ifdef DEBUG
                                        printf("[SR SENDER] Timeout detected for specific packet: %d. Retransmitting alone.\n", check_ptr);
                                        #endif
                                        struct srtp_header_t retransmit_header = {
                                                .syn = 0, .fin = 0,
                                                .seq = check_ptr,
                                                .ack_flag = 0, .nack = 0, .ack_num = 0,
                                                .length = (uint8_t)file_sizes[buf_idx],
                                                .crc32 = 0
                                        };
                                        uint8_t * data_segment = srtp_data(retransmit_header, file_data[buf_idx]);
                                        sendto(sockfd_data, data_segment, 9 + retransmit_header.length, 0, (struct sockaddr*)dest_addr, len);
                                        free(data_segment);

                                        gettimeofday(&timers[buf_idx], NULL); // Reset individual timer
                                }
                        }
                        check_ptr = (check_ptr + 1) & 0x3FFF;
                }

                if (end_of_file && base == next_seq) {
                        #ifdef DEBUG
                        printf("[SR SENDER] All segments successfully acknowledged. Exiting main loop.\n");
                        #endif
                        break;
                }

                n = recvfrom(sockfd_ack, response, 9, 0, (struct sockaddr *)dest_addr, &len);
                if (n < 0) continue; // Timeouts managed individually above

                checksum_b = srtp_checksum(response, n);

                if (checksum_b) {
                        ack_count = (response[2] & 0x3F) << 8;
                        ack_count |= response[3];

                        uint8_t is_ack = (response[2] & 0x80) == 0x80;
                        uint8_t is_nack = (response[2] & 0x40) == 0x40;

                        uint16_t dist = (uint16_t)(ack_count - base) & 0x3FFF;

                        if (is_ack && (dist < window_size)) {
                                int buf_idx = ack_count % window_size;
                                #ifdef DEBUG
                                printf("[SR SENDER] Individual ACK received for packet: %d.\n", ack_count);
                                #endif
                                ack_flags[buf_idx] = 1; // Mark this specific packet as confirmed

                                // Slide window forward if base packet got acknowledged
                                while (base != next_seq && ack_flags[base % window_size]) {
                                        base = (base + 1) & 0x3FFF;
                                }
                        }
                        else if (is_nack && (dist < window_size)) {
                                int buf_idx = ack_count % window_size;
                                #ifdef DEBUG
                                printf("[SR SENDER] Explicit NACK received for packet: %d. Fast retransmitting.\n", ack_count);
                                #endif
                                struct srtp_header_t nack_header = {
                                        .syn = 0, .fin = 0,
                                        .seq = ack_count,
                                        .ack_flag = 0, .nack = 0, .ack_num = 0,
                                        .length = (uint8_t)file_sizes[buf_idx],
                                        .crc32 = 0
                                };
                                uint8_t * data_segment = srtp_data(nack_header, file_data[buf_idx]);
                                sendto(sockfd_data, data_segment, 9 + nack_header.length, 0, (struct sockaddr*)dest_addr, len);
                                free(data_segment);

                                gettimeofday(&timers[buf_idx], NULL); // Restart timer after NACK repair
                        }
                }
        }

        for(int i = 0; i < window_size; i++){
                free(file_data[i]);
        }
        free(file_data);

        return (int)((base - 1) & 0x3FFF); // Must reduce in one
}

/*
 * SRTP_Send in Go-Back-N Mode
 * Keeps a window of data (to send in case its needed)
 * 
 */
int srtp_send_gbn(int sockfd_data, int sockfd_ack, FILE * file, const struct sockaddr_in *dest_addr, uint8_t window_size){

        uint8_t file_segmenets[255];
        int len = sizeof(struct sockaddr_in);
        int n = 0;              //N Bytes from recvfrom()
        int checksum_b = 0;     //Boolean for Checksum

        uint8_t ** file_data = malloc(sizeof(uint8_t *) * window_size);
        for(int i = 0; i < window_size; i++){
                file_data[i] = malloc(sizeof(uint8_t) * 255);
        }

        /* Cleans memory for file_data to store */
        for(int i = 0; i < window_size; i++){
                for(int j = 0; j < 255; j++){
                        file_data[i][j] = 0;
                }
        }

        uint8_t response[9];    //Used in ACK/NACKs
        size_t bytes_readen = 0;
        uint16_t base = 0;
        uint16_t next_seq = 0;
        uint8_t  end_of_file = 0;       // Boolean to recognize EOF
        uint16_t file_sizes[window_size]; // Buffer used to know packet size for sendto()

        uint16_t ack_count = 0;

        uint16_t send_ptr = 0; // Pointer for next file

        while(1){
                while (((uint16_t)(next_seq - base) & 0x3FFF) < window_size && !end_of_file) {
                        bytes_readen = fread(file_segmenets, 1, 255, file);
                        if (bytes_readen > 0) {
                                int buf_idx = next_seq % window_size;           
                                file_sizes[buf_idx] = (uint16_t)bytes_readen;   
                                for (int j = 0; j < bytes_readen; j++) {
                                        file_data[buf_idx][j] = file_segmenets[j];
                                }
                                next_seq = (next_seq + 1) & 0x3FFF; 
                        } else {
                                end_of_file = 1;
                        }
                }

                // Implementation of GBN
                // Send Packets (In Window Size)
                while (send_ptr != next_seq) {
                        int buf_idx = send_ptr % window_size;
                        
                        struct srtp_header_t segment_header = {
                                .syn = 0, .fin = 0,
                                .seq = send_ptr, 
                                .ack_flag = 0, .nack = 0, .ack_num = 0,
                                .length = (uint8_t)file_sizes[buf_idx],
                                .crc32 = 0 
                        };

                        uint8_t * data_segment = srtp_data(segment_header, file_data[buf_idx]);
                        sendto(sockfd_data, data_segment, 9 + segment_header.length, 0, (struct sockaddr*)dest_addr, len);
                        free(data_segment);
                        
                        send_ptr = (send_ptr + 1) & 0x3FFF;
                }

                if (end_of_file && base == next_seq) {
                        #ifdef DEBUG
                        printf("[GBN SENDER] All segments successfully acknowledged. Exiting main loop.\n");
                        #endif
                        break;
                }

                n = recvfrom(sockfd_ack, response, 9, 0, (struct sockaddr *)dest_addr, &len);
                if (n < 0) 
                {
                        #ifdef DEBUG
                        printf("[GBN SENDER] Timeout to base value %d...\n", base);
                        #endif
                        send_ptr = base; // GO Back N in Timeout
                        continue;
                }

                checksum_b = srtp_checksum(response, n);

                // NACK Treatement
                if (checksum_b) {
                        ack_count = (response[2] & 0x3F) << 8;
                        ack_count |= response[3];

                        uint8_t is_ack = (response[2] & 0x80) == 0x80;
                        uint8_t is_nack = (response[2] & 0x40) == 0x40;

                        if (is_ack) {
                                uint16_t dist = (uint16_t)(ack_count - base) & 0x3FFF;
                                
                                if (dist < window_size) {
                                        #ifdef DEBUG
                                        printf("[GBN SENDER] Valid Cumulative ACK received: %d.\n", ack_count);
                                        #endif
                                        base = (ack_count + 1) & 0x3FFF; 
                                        
                                        // Sincroniza o send_ptr caso a base avance além dele
                                        uint16_t send_dist = (uint16_t)(send_ptr - base) & 0x3FFF;
                                        if (send_dist > window_size) {
                                                send_ptr = base;
                                        }
                                }
                        }
                        else if (is_nack) {
                                #ifdef DEBUG
                                printf("[GBN SENDER] NACK received! Rolling back send pointer to expected packet: %d.\n", ack_count);
                                #endif
                                send_ptr = ack_count; // FIXED: No NACK, recua o ponteiro para o ponto do erro imediatamente
                        }
                }
        }

        for(int i = 0; i < window_size; i++){
                free(file_data[i]);
        }
        free(file_data);
        return base;
}

/*
 * SRTP_Send in Stop-and-Wait Mode
 * Sends 1 Packet -> Wait for ACK -> (In case of timeout, sends again)
 * No reaction to NACKs
 */
int srtp_send_saw(int sockfd_data, int sockfd_ack, FILE * file, const struct sockaddr_in *dest_addr, uint8_t window_size){
        uint8_t file_segmenent[255];
        size_t bytes_lidos = 0;
        int len = sizeof(struct sockaddr_in);
        uint8_t data_received_correctly = 0;
        int n = 0;
        int checksum_b = 0;

        struct sockaddr_in from_addr;

        uint8_t response[9];
        uint16_t seq_count = 0;

        while((bytes_lidos = fread(file_segmenent, 1, 255, file)) > 0){
                data_received_correctly = 0;

                while(!data_received_correctly){
                        struct srtp_header_t segement_header = {
                                .syn = 0,
                                .fin = 0,
                                .seq = seq_count,
                                .ack_flag = 0,
                                .nack = 0,
                                .ack_num = 0,
                                .length = (uint8_t)bytes_lidos,
                                .crc32 = 0 
                        };

                        uint8_t * data_segmenet = srtp_data(segement_header, file_segmenent);
                        sendto(sockfd_data, data_segmenet, 9 + segement_header.length, 0, (struct sockaddr*)dest_addr, len);
                        free(data_segmenet);

                        n = recvfrom(sockfd_ack, response, 9, 0, (struct sockaddr *)&from_addr, &len);
                        
                        if(n < 0){
                                // Retransmits Packet 
                                continue; 
                        }

                        checksum_b = srtp_checksum(response, n);

                        if(checksum_b){
                                uint8_t  ack_flag  = (response[2] >> 7) & 0x01;
                                uint8_t  nack_flag = (response[2] >> 6) & 0x01;
                                uint16_t ack_num   = ((response[2] & 0x3F) << 8) | response[3];

                                // Must be zero for correct response (0x00 0x00 ACK_DATA Lenght=0 ...)
                                if((response[0] == 0x00) && response[1] == 0x00 && (response[4] == 0x0)){
                                        // Ack Count Equal and ACK_FLAG = 1
                                        if(ack_flag && !nack_flag && (ack_num == seq_count)){
                                                data_received_correctly = 1;
                                        }       
                                }
                        }
                }

                // Limpo o buffer de envio por garantia
                for(int i = 0; i < 255; i++){
                        file_segmenent[i] = 0;
                }

                seq_count++;
                if(seq_count > 16383) seq_count = 0;
        }

        return seq_count;
}

/*
 * Work as Wrapper for the modes for SRTP_Send
 */
int srtp_send(int sockfd_data, int sockfd_ack, FILE *file, const struct sockaddr_in *dest_addr, uint8_t window_size, int mode){
        uint16_t seq_count_return = 0;

        switch (mode)
        {
                case GO_BACK_N_MODE:
                        seq_count_return = srtp_send_gbn(sockfd_data, sockfd_ack, file, dest_addr, window_size);
                break;
                case SELETIVE_REPEAT:
                        seq_count_return = srtp_send_sr(sockfd_data, sockfd_ack, file, dest_addr, window_size);
                break;
        default:
                // Stop and Wait for Default
                seq_count_return = srtp_send_saw(sockfd_data, sockfd_ack, file, dest_addr, window_size);
                break;
        }

        return seq_count_return;
}

/*
 * SRTP_Close - Used by Host's API
 * Returns:
 * 0 - Success
 * 1 - Timeout? (Must Implement) 
 */
int srtp_close(int sockfd_data, int sockfd_ack, struct sockaddr_in * dest_addr, uint16_t last_seq_count){ 
        // Sends a transmission informing that data is over
        int len = sizeof(struct sockaddr_in);
        
        int n = 0;
        int checksum_b = 0;

        uint16_t last_val = 0;
        //last_val = last_seq_count; // Change in case Last Seq is used to Finish Comunication

        struct srtp_header_t fin_header = {
                .syn      = 0,
                .fin      = 1,                 
                .seq      = last_val,    
                .ack_flag = 0,
                .nack     = 0,
                .ack_num  = 0,
                .length   = 0,
                .crc32    = 0
        };


        #ifdef DEBUG
        printf("[SENDER] Trying to finish coommunication.\n");
        #endif

        struct sockaddr_in from_addr;

        uint8_t response[9];
        uint8_t close_confirmed = 0;
        int timeout = 0;
        while(!close_confirmed){
                if(timeout == MAX_TRIES){
                        #ifdef DEBUG
                        printf("[SENDER] Timeout - Finishing comunication in order to prevent infinite local loop\n");
                        #endif
                        break;
                }
                uint8_t * finish_data = srtp_data(fin_header, 0);
                sendto(sockfd_data, finish_data, 9, 0, (struct sockaddr*)dest_addr, len);
                free(finish_data);

                n = recvfrom(sockfd_ack, response, 9, 0, (struct sockaddr *)&from_addr, &len);
                if(n < 0){
                        timeout++; 
                        continue; 
                }
                checksum_b = srtp_checksum(response, n);
                if(checksum_b){
                        uint8_t fin_flag = (response[0] & 0x40) == 0x40; 
                        uint8_t ack_flag = (response[2] & 0x80) == 0x80; 
                        uint16_t ack_num = (response[2] & 0x3F) << 8;
                        ack_num |= response[3];        

                        if (fin_flag && ack_flag && (ack_num == last_val)) {
                                close_confirmed = 1;
                                #ifdef DEBUG
                                printf("[SENDER] Success on termination of communication via FIN+ACK.\n");
                                #endif
                        }
                }
        }

        return 0;
}

/*
 * SRTP Receive in Seletive-Repeat (Mode)
 * Returns Last_Seq_Count (used for closing connection later)
 */
int srtp_receive_sr(int sockfd_data, uint16_t port_in, FILE * file_output, struct sockaddr_in * source_addr, uint8_t window_size){
        uint8_t receive_buffer[BUFFER_SIZE];
        uint16_t file_counter = 0;
        uint16_t last_file_counter = 0;
        uint16_t seq_counter = 0;

        uint8_t * response_to_otherside;
        int n = 0;
        int check = 0;
        int len = sizeof(struct sockaddr_in);

        // SR Buffering Matrix Allocation
        uint8_t ** rx_window = malloc(sizeof(uint8_t *) * window_size);
        for(int i = 0; i < window_size; i++){
                rx_window[i] = malloc(sizeof(uint8_t) * 255);
        }
        uint8_t rx_filled[window_size];
        uint16_t rx_sizes[window_size];
        for(int i = 0; i < window_size; i++){
                rx_filled[i] = 0;
                rx_sizes[i] = 0;
        }

        while(1){
                len = sizeof(struct sockaddr_in);
                n = recvfrom(sockfd_data, receive_buffer, BUFFER_SIZE, 0, (struct sockaddr *)source_addr, &len);
                if(n < 0) continue;

                check = srtp_checksum(receive_buffer, n);
                
                if(check){
                        seq_counter = (receive_buffer[0] & 0x3F) << 8;
                        seq_counter |= receive_buffer[1]; 

                        struct srtp_header_t header = { 
                                .syn  = 0, .fin  = 0, .seq  = 0, .nack = 0,
                                .ack_flag = 0, .ack_num = 0,
                                .length = (uint8_t)(n - 9),
                                .crc32 = 0 
                        };

                        uint8_t payload_len = receive_buffer[4];
                        uint8_t is_fin = (receive_buffer[0] & 0x40) == 0x40;

                        source_addr->sin_port = htons(port_in + 1);

                        if (is_fin) {
                                #ifdef DEBUG
                                printf("[SR RECEIVER] FIN frame intercepted. Ending stream processing.\n");
                                #endif
                                break;
                        }

                        // Determine circular distance from expected base counter
                        uint16_t dist = (uint16_t)(seq_counter - file_counter) & 0x3FFF;

                        // Case A: Packet is inside the current window range
                        if (dist < window_size) {
                                int buf_idx = seq_counter % window_size;
                                
                                // Store packet payload in buffer if not already stored
                                if (!rx_filled[buf_idx]) {
                                        rx_sizes[buf_idx] = payload_len;
                                        for (int j = 0; j < payload_len; j++) {
                                                rx_window[buf_idx][j] = receive_buffer[9 + j];
                                        }
                                        rx_filled[buf_idx] = 1;
                                        #ifdef DEBUG
                                        printf("[SR RECEIVER] Buffered packet: %d (Window index slot: %d).\n", seq_counter, buf_idx);
                                        #endif
                                }

                                // Always ACK the packet individually
                                header.ack_flag = 1;
                                header.ack_num = seq_counter;
                                response_to_otherside = srtp_data(header, 0);
                                sendto(sockfd_data, response_to_otherside, 9, 0, (struct sockaddr *)source_addr, len);
                                free(response_to_otherside);

                                // Flush contiguous, sequential buffered packets to disk
                                while (rx_filled[file_counter % window_size]) {
                                        int flush_idx = file_counter % window_size;
                                        if (rx_sizes[flush_idx] > 0) {
                                                fwrite(rx_window[flush_idx], 1, rx_sizes[flush_idx], file_output);
                                                fflush(file_output);
                                        }
                                        #ifdef DEBUG
                                        printf("[SR RECEIVER] Flushed packet %d from buffer to file.\n", file_counter);
                                        #endif
                                        
                                        rx_filled[flush_idx] = 0; // Reset slot
                                        last_file_counter = file_counter;
                                        file_counter = (file_counter + 1) & 0x3FFF; // Advance base
                                }
                        }
                        // Case B: Packet is an old duplicate from a previous window (ACK dropped in transit)
                        else if (((uint16_t)(file_counter - seq_counter) & 0x3FFF) <= window_size) {
                                #ifdef DEBUG
                                printf("[SR RECEIVER] Old packet duplicate detected: %d. Re-sending individual ACK.\n", seq_counter);
                                #endif
                                header.ack_flag = 1;
                                header.ack_num = seq_counter;
                                response_to_otherside = srtp_data(header, 0);
                                sendto(sockfd_data, response_to_otherside, 9, 0, (struct sockaddr *)source_addr, len);
                                free(response_to_otherside);
                        }
                        // Case C: Completely out of bounds packet, issue NACK for the missing base frame
                        else {
                                #ifdef DEBUG
                                printf("[SR RECEIVER] Completely out of bounds frame! Expected base: %d, got: %d. Sending NACK.\n", file_counter, seq_counter);
                                #endif
                                header.nack = 1;
                                header.ack_num = file_counter;
                                response_to_otherside = srtp_data(header, 0);
                                sendto(sockfd_data, response_to_otherside, 9, 0, (struct sockaddr *)source_addr, len);
                                free(response_to_otherside);
                        }
                }
        }

        for(int i = 0; i < window_size; i++){
                free(rx_window[i]);
        }
        free(rx_window);

        return last_file_counter;
}

/*
 * SRTP Receive in Go-Back-N (Mode)
 * Returns Last_Seq_Count (used for closing connection later)
 */
/*
 * SRTP Receive in Go-Back-N (Mode)
 */
int srtp_receive_gbn(int sockfd_data, uint16_t port_in, FILE * file_output, struct sockaddr_in * source_addr, uint8_t window_size){
        uint8_t receive_buffer[BUFFER_SIZE];
        uint16_t file_counter = 0;
        uint16_t last_file_counter = 0;
        uint16_t seq_counter = 0;

        uint8_t * response_to_otherside;
        int n = 0;
        int check = 0;
        int len = sizeof(struct sockaddr_in);

        while(1){
                len = sizeof(struct sockaddr_in);
                n = recvfrom(sockfd_data, receive_buffer, BUFFER_SIZE, 0, (struct sockaddr *)source_addr, &len);
                if(n < 0) continue;

                check = srtp_checksum(receive_buffer, n);
                
                if(check){
                        seq_counter = (receive_buffer[0] & 0x3F) << 8;
                        seq_counter |= receive_buffer[1]; 

                        struct srtp_header_t header = { 
                                .syn  = 0,
                                .fin  = 0,
                                .seq  = 0,
                                .nack = 0,
                                .ack_flag = 0,
                                .ack_num = 0,
                                .length = (uint8_t)(n - 9),
                                .crc32 = 0 
                        };

                        uint8_t payload_len = receive_buffer[4];
                        uint8_t is_fin = (receive_buffer[0] & 0x40) == 0x40;

                        source_addr->sin_port = htons(port_in + 1);

                        if (is_fin) {
                                #ifdef DEBUG
                                printf("[GBN RECEIVER] FIN frame intercepted. Ending stream processing.\n");
                                #endif
                                break;
                        }

                        // Correct Order File
                        if (seq_counter == file_counter) {
                                header.ack_flag = 1;
                                header.nack = 0;
                                header.ack_num = file_counter;

                                #ifdef DEBUG
                                printf("[GBN RECEIVER] Packet in order: %d. Writing to file.\n", seq_counter);
                                #endif

                                if (payload_len > 0) {
                                        fwrite(&receive_buffer[9], 1, payload_len, file_output);
                                        fflush(file_output);
                                }

                                response_to_otherside = srtp_data(header, 0);
                                sendto(sockfd_data, response_to_otherside, 9, 0, (struct sockaddr *)source_addr, len);
                                free(response_to_otherside);

                                last_file_counter = file_counter;
                                file_counter = (file_counter + 1) & 0x3FFF;
                        } 

                        // In case of out of order
                        else {
                                uint16_t ultimo_correto = (file_counter == 0) ? 16383 : (file_counter - 1);

                                header.ack_flag = 1; 
                                header.nack = 0;    
                                header.ack_num = ultimo_correto; 

                                #ifdef DEBUG
                                printf("[GBN RECEIVER] Out of order! Expected %d, got %d. Sending ACK duplicate for %d.\n", 
                                       file_counter, seq_counter, ultimo_correto);
                                #endif

                                response_to_otherside = srtp_data(header, 0);
                                sendto(sockfd_data, response_to_otherside, 9, 0, (struct sockaddr *)source_addr, len);
                                free(response_to_otherside);
                        }
                }
        }
        return last_file_counter;
}

/*
 * Receives in Stop-And-Wait Mode
 * Return Last Seq Count (must have in order to finish communication properly)
 */
int srtp_receive_saw(int sockfd_data, uint16_t port_in, FILE * file_output, struct sockaddr_in * source_addr, uint8_t window_size){
        uint8_t receive_buffer[BUFFER_SIZE];
        uint16_t file_counter = 0;
        uint16_t last_file_counter = 0; // Used in SRTP_Close
        uint16_t seq_counter = 0;

        uint8_t * response_to_otherside;
        int n = 0;
        int check = 0;
        int len = sizeof(struct sockaddr_in);

        while(1){
                len = sizeof(struct sockaddr_in);
                n = recvfrom(sockfd_data, receive_buffer, BUFFER_SIZE, 0, (struct sockaddr *)source_addr, &len);
                if(n < 0) continue;

                check = srtp_checksum(receive_buffer, n);
                
                if(check){
                        seq_counter = (receive_buffer[0] & 0x3F) << 8;
                        seq_counter |= receive_buffer[1]; 

                        struct srtp_header_t header = { 
                                .syn  = 0,
                                .fin  = 0,
                                .seq  = 0,
                                .nack = 0,
                                .ack_flag = 0,
                                .ack_num = seq_counter,
                                .length = 0,    // Lenght == 0 since its only an ACK with no payload
                                .crc32 = 0 
                        };

                        uint8_t payload_len = receive_buffer[4];

                        // Ack forced on P+1
                        source_addr->sin_port = htons(port_in + 1);

                        // Seq = 0, Only FIN in first segement
                        if(((receive_buffer[0] == (0x40)) && (receive_buffer[1] == 0x00)) && (payload_len == 0)){
                                fclose(file_output);
                                break;
                        }

                        // Correct Send Process
                        if((file_counter) == seq_counter){
                                header.ack_flag = 1;

                                if(payload_len > 0){
                                        fwrite(&receive_buffer[9], 1, payload_len, file_output);
                                        fflush(file_output);
                                }

                                response_to_otherside = srtp_data(header, 0); //No Payload since its a response
                                sendto(sockfd_data, response_to_otherside, 9, 0, (struct sockaddr *)source_addr, len);
                                free(response_to_otherside);

                                // Send ACK
                                last_file_counter = file_counter;
                                file_counter++;
                                if(file_counter > 16383) file_counter = 0;
                        }else{
                                // Correct CRC, but with it being out of order or repeated, just sends ACK and continue (without increasing order)
                                header.ack_flag = 1;

                                response_to_otherside = srtp_data(header, 0); //No Payload since its a response
                                sendto(sockfd_data, response_to_otherside, 9, 0, (struct sockaddr *)source_addr, len);
                                free(response_to_otherside);
                        }
                }
        }
        return last_file_counter;
}

/* 
 * API Function used to receive FILE from another Host (and to write in a output file)
 * Works as a Wrapper
 */
int srtp_receive(int sockfd_data, uint16_t port_in, FILE * file_output, struct sockaddr_in * source_addr, uint8_t window_size, int mode){
        uint16_t last_file_counter = 0;
        switch (mode)
        {
                case GO_BACK_N_MODE:
                        last_file_counter = srtp_receive_gbn(sockfd_data, port_in, file_output, source_addr, window_size);
                break;
                case SELETIVE_REPEAT:
                        last_file_counter = srtp_receive_sr(sockfd_data, port_in, file_output, source_addr, window_size);
                break;
        default:
                // Stop and Wait for Default
                last_file_counter = srtp_receive_saw(sockfd_data, port_in, file_output, source_addr, window_size);
                break;
        }

        last_file_counter = 0; // Change in case Last Seq is used to Finish Comunication

        // Finalizes Communicatin send FIN+ACK
        struct srtp_header_t finish_header = {
                .syn = 0,
                .fin = 1,
                .seq   = 0,
                .ack_flag = 1,
                .ack_num = last_file_counter,
                .nack = 0,
                .length = 0,
                .crc32 = 0
        };

        // Forces ACK to Port P+1
        source_addr->sin_port = htons(port_in + 1);

        int len = sizeof(struct sockaddr_in);
        uint8_t * finish_communication = srtp_data(finish_header, 0);
        sendto(sockfd_data, finish_communication, 9, 0, (struct sockaddr *)source_addr, len);
        free(finish_communication);

        #ifdef DEBUG
        printf("[LISTEN] Connection finished with success - Sent FIN+ACK.\n");
        #endif
        
        return 0;
}
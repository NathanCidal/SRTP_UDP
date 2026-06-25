#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "zlib.h" // Utilizada para fazer calculo do CRC32

#define PORT 5000
#define MAXLINE 1000
#define BUFFER_SIZE 1024

#define SEND 1
#define RECEIVED 0

#define IS_HOST 1
#define IS_SERVER 0

/**************
 * Pode se afirmar que todos as mensagens + payload ocupam 255 Bytes + 9 Bytes (Header)
 **************/

/*************************** 
 * Formato do Header RTP *
 * - Opera sobre UDP
 * - Comunicacao Half-Duplex (possui ACK,NACK)
 
 +0+++++++++++++++++++1+++++++++++++++++++2+++++++++++++++++++3+++++
 +0+1+2+3+4+5+6+7+8+9+0+1+2+3+4+5+6+7+8+9+0+1+2+3+4+5+6+7+8+9+0+1+2+
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |S|F|       SEQ (14 bits)     |A|N|         ACK (14 bits)         |  
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |Lenght (8 Bits)|                 CRC32 (32 Bits) ...             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |CRC32 (32 Bits)| ############################################### | 
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 * - Lenght = 255 - pacote intermediario; o receiver bufferiza e aguarda;
 * - Lenght < 255 - ultimo pacote do stream; o receiver entrega o buffer completo a aplicação (push).
 * - Lenght = 0   - edge case: o arquivo e multiplo exato de 255, sinaliza fim de stream sem payload residual.
 * - Durante o Handshake (pacotes SYN e SYN+ACK): tamanho de janela proposto pelo lado que envia.
 
 * - Contado em pacotes, nao em bytes
 * - Inicia em 0 no primeiro pacote de dados apos o handshake;
 * - Espaco de sequencia: 14 bits, wrap-around deve ser tratado adequadamente
 * - Todos pacotes tem payload de 255 bytes, exceto o ultimo pacote do stream
 
 * - Checksum CRC32

 * Estabelecimento de conexao (three-way handshake) * 
  Iniciador                             Receiver
      |                                    |
      |--- SYN (Lenght=janela_i) --------> |
      |                                    |
      |<-- SYN+ACK (Lenght=janela_r) ------|
      |                                    |
      |--- ACK ----------------------------|
      |                                    |
      |  [Transferencia de Dados]          |

 * Finalizacao da Conexao (two-way) *
  Sender                                  Receiver
    |                                         |
    |--- FIN (Lenght=0) --------------------> |
    |                                         |
    |<-- FIN+ACK -----------------------------|
    |                                         |

    Exemplo de execucao:
    // Receiver escutando
    ./executavel --listen --port 6000

    // Sender conectado ao Receiver
    ./executavel --host 192.168.1.10 --port 6000 --file arquivo.bin

    Nesse protocolo, podemos dizer que o Receive e o "Listen", 
    E que o "Host", quem vai enviar, e o "--host".

    Se estima que, para uma comunicacao half-duplex, ou seja, um por vez
    seja necessario que ambos os lados se comportem como server no Protocolo UDP.

    Estudando isso atualmente.

    Para os testes, primeiro vou considerar tudo em Loopback, ou seja, nenhum erro.
    Depois vou colocar as medidas para "proteger" o algoritmo.

    Tera 3 modos de operacao:
    SAW | GBN | SR
*/

// Used only for Debug propose
void print_bin(uint8_t d){
    for(int i = 7; i >= 0; i--){
        if(d & (1 << i)) printf("1");
        else printf("0");
    }
}

// Host (Client) = 1  | Listen (Server) = 0
// Send = 1 -> Sender | Send = 0 -> Receive
void print_pacote_bin(uint8_t * pacote, int size, int host, int send){
    if(host) printf("Host");
    else printf("Server");

    if(send) printf(" - Recebi ");
    else printf(" - Enviei ");
    
    printf("o seguinte pacote: \n");

    for(int i = 0; i < size; i++){
        printf("|");
        print_bin(pacote[i]);
    }
    printf("|\n");
}


void print_pacote_analisado(uint8_t * pacote){
    uint8_t aux = pacote[0];
    
    uint8_t  syn = pacote[0] & 0b10000000;
    uint8_t  fin = pacote[0] & 0b01000000;
    uint16_t seq = ((pacote[0] & 0x3F) << 8) | pacote[1]; 
    uint8_t  ack_flag = pacote[2] & 0b10000000;
    uint8_t nack_flag = pacote[2] & 0b01000000;
    uint16_t ack_count = ((pacote[2] & 0x3F) << 8) | pacote[3]; 
    uint8_t lenght = pacote[4];
    uint32_t crc32 = (pacote[5] << 24) | (pacote[6] << 16) | (pacote[7] << 8) | pacote[8];

    printf("-> SYN = %d      | -> FIN = %d | -> SEQ       = %05d |\n", syn == 0x80, fin == 0x40, seq);
    printf("-> ACK = %d      | -> NAK = %d | -> ACK_Count = %05d |\n", ack_flag == 0x80, nack_flag == 0x40, ack_count);
    printf("-> Lenght = %03d | -> CRC = %u\n\n", lenght, crc32);
}


/** 
 * Verifica se o CRC32 esta correto
 */
uint8_t checksum_verifier_header(uint8_t * header_pointer, int size){
    uint8_t * aux_header = malloc(sizeof(uint8_t) * size);
    for(int i = 0; i < size; i++){
        aux_header[i] = header_pointer[i];
    }

    for(int i = 0; i < 4; i++){
        aux_header[i+5] = 0;
    }

    uint32_t checksum_calculated = (uint32_t)crc32(0, aux_header, size);

    // Converter para Little-Endian
    uint32_t checksum_received =
        ((uint32_t)header_pointer[5] << 24) |
        ((uint32_t)header_pointer[6] << 16) |
        ((uint32_t)header_pointer[7] <<  8) |
        ((uint32_t)header_pointer[8] <<  0);    

    #ifdef PRINT   
    printf("(Received) %x - (Calculated) %x\n", checksum_received, checksum_calculated);
    #endif

    return (checksum_received == checksum_calculated);
}

uint8_t * rtp_data(int syn, int fin, int seq, int ack, 
                    int ack_flag, int nack, int length, 
                    int crc, uint8_t * payload)
{
    uint8_t * header_aux = malloc(sizeof(uint8_t) * (9 + length));

    // 2 Bytes
    header_aux[0]  = (uint8_t)((seq >> 8) & 0x3F);
    header_aux[0] |= (uint8_t)(((syn << 1) | (fin << 0)) << 6);
    header_aux[1]  = (uint8_t)(seq & 0xFF);

    // 2 Bytes
    header_aux[2]  = (uint8_t)((ack >> 8) & 0x3F);
    header_aux[2] |= (uint8_t)(((ack_flag << 1) | (nack << 0)) << 6);
    header_aux[3]  = (uint8_t)(ack & 0xFF);

    // 1 Byte
    header_aux[4] = (uint8_t)(length);

    for(int i = 0; i < 4; i++){
        header_aux[i+5] = 0;
    }

    for(int i = 0; i < length; i++){
        header_aux[i+9] = payload[i];
    }

    uint32_t crc_calculated = (uint32_t)crc32(crc, header_aux, (length+9));

    // 4 Bytes
    // CRC em Big Endian
    header_aux[5] = (crc_calculated >> 24) & 0xFF;
    header_aux[6] = (crc_calculated >> 16) & 0xFF;
    header_aux[7] = (crc_calculated >>  8) & 0xFF;
    header_aux[8] = (crc_calculated >>  0) & 0xFF;

    #ifdef PRINT
        printf("Hexa: \n");
        for(int i = 0; i < 9; i++){
            if(i % 4 == 0) printf("\n");
            printf("%02x ", header_aux[i]);    
        }
        printf("\n");
        printf("Bin: \n");
        for(int i = 0; i < 9; i++){
        if(i % 4 == 0) printf("\n");
        print_bin(header_aux[i]);
        printf(" ");
    }
    #endif

    return header_aux;
}

int interface_host(){
    // Iniciador (Cliente)
    int sockfd;
    struct sockaddr_in servaddr;
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("\n Error : Connect Failed \n");
        exit(0);
    }


    // Propoe janela de 1 Byte de Inicio
    // Funcoes abaixos podem ser inseridas como init_connection_client(int janela_size)
    uint8_t window_size_client = 255;
    uint8_t window_size_communication = window_size_client;
    uint8_t * data_client;
    uint8_t buffer_client[BUFFER_SIZE];
    
    // Primeira Etapa do Handshake (Iniciador manda SYN para Receiver)
    data_client = rtp_data(1, 0, 0x0, 0, 0, 0, window_size_client, 0);  

    //print_pacote_bin(data_client, 9, IS_HOST, SEND);
    printf("Host - Envia: \n");
    print_pacote_analisado(data_client);
    
    sendto(sockfd, data_client, 9,  0, (struct sockaddr*)NULL, sizeof(servaddr));
    free(data_client);

    // Segunda Etapa do Handshake (Receiver envia SYN+ACK para o Iniciador, junto do tamanho da janela)
    recvfrom(sockfd, buffer_client, BUFFER_SIZE, 0, (struct sockaddr*)NULL, NULL);
    //print_pacote_bin(data_client, 9, IS_HOST, RECEIVED);
    
    printf("Host - Recebe: \n");
    print_pacote_analisado(buffer_client);


    int b = checksum_verifier_header(buffer_client);
    if(b == 0){
        printf("Inicializacao de Comunicacao Falha\n");
        close(sockfd);
        return 1;
    }

    if(buffer_client[5] < window_size_client){
        window_size_communication = buffer_client[5];
    }

    data_client = rtp_data(0, 0, 0x0, 1, 0, 0, 0, 0);

    // print_pacote_bin(data_client, 9, IS_HOST, SEND);
    printf("Host - Envia: \n");
    print_pacote_analisado(data_client);

    sendto(sockfd, data_client, 9,  0, (struct sockaddr*)NULL, sizeof(servaddr));
    free(data_client);

    // Comunicacao
    FILE * fp = fopen("arquivo.txt", "r");
    fp = 0;
    if(!fp){
        // Finaliza comunicacao
        goto fim_host;
    }

    uint16_t    lenght_sender = 0;
    uint16_t ack_count_sender = 0;
    uint16_t seq_count_sender = 0;

fim_host:
    // Comunicacao FIN
    data_client = rtp_data(0, 1, seq_count_sender, ack_count_sender, 0, 0, 0, 0x0);
    sendto(sockfd, data_client, 9,  0, (struct sockaddr*)NULL, sizeof(servaddr));
    
    printf("Host - Envia: \n");
    print_pacote_analisado(data_client);
    
    free(data_client);
    
    close(sockfd);
    return 0;
}

int interface_listen(){
    // Receiver (Se comporta como Servidor)
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    int listenfd, len;
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);        
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET; 
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));  
    len = sizeof(cliaddr);

    // Funcoes abaixos podem ser inseridas como init_connection_server(int janela_size)
    uint8_t window_size_server        = 255;
    uint8_t window_size_communication = window_size_server;
    uint8_t buffer_server[BUFFER_SIZE];
    uint8_t * data_server;

    // Recebe o Receive do Cliente
    // n is used to indicate the size of data received
    int n = recvfrom(listenfd, buffer_server, BUFFER_SIZE, 0, (struct sockaddr*)&cliaddr,&len); //receive message from server
    int b = checksum_verifier_header(buffer_server);

    // print_pacote_bin(buffer_server, 9, IS_SERVER, RECEIVED);
    printf("Client - Recebe: \n");
    print_pacote_analisado(buffer_server);

    if(b == 0){
        printf("Inicializacao de Comunicacao Falha\n");
        return 1;
    }

    if(buffer_server[5] < window_size_server){
        window_size_communication = buffer_server[5];
    }
    
    data_server = rtp_data(1, 0, 0x0, 0, 1, 0, window_size_server, 0);
    sendto(listenfd, data_server, 9, 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
    
    // print_pacote_bin(data_server, 9, IS_SERVER, SEND);
    printf("Client - Envia: \n");
    print_pacote_analisado(data_server);

    free(data_server);

    n = recvfrom(listenfd, buffer_server, BUFFER_SIZE, 0, (struct sockaddr*)&cliaddr,&len); //receive message from server
    b = checksum_verifier_header(buffer_server);

    printf("Client - Recebe: \n");
    print_pacote_analisado(buffer_server);

    if(b == 0){
        printf("Inicializacao de Comunicacao Falha\n");
        return 1;
    }else{
        printf("Tudo certinho!\n");
    }

    int keep_communication = 0;
    // Listen
    while(1){
        n = recvfrom(listenfd, buffer_server, BUFFER_SIZE, 0, (struct sockaddr*)&cliaddr,&len); //receive message from server
        b = checksum_verifier_header(buffer_server);
        
        // N e o numero de bytes recebidos, ou seja, vai ir de 9 ate 264 (9b + 255b)

        if(buffer_server[0] & 0x40)
    }

    return 0;
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
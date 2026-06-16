#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 5000
#define MAXLINE 1000
#define BUFFER_SIZE 1024

// #define PRINT

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
*/

// Used only for Debug propose
void print_bin(uint8_t d){
    for(int i = 7; i >= 0; i--){
        if(d & (1 << i)) printf("1");
        else printf("0");
    }
}

uint8_t * rtp_header(int syn, int fin, int seq, int ack, 
                     int ack_flag, int nack, int length, 
                    int crc)
{
    uint8_t * header_aux = malloc(sizeof(uint8_t) * 9);
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

    // 4 Bytes
    for(int i = 0; i < 4; i++){
        header_aux[8-i] = (uint8_t)((crc >> (8*i) & 0xFF));
    }

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
    int sockfd, n;
    uint8_t buffer[BUFFER_SIZE];
    
    // Propoe janela de 1 Byte de Inicio
    uint8_t * data = rtp_header(1, 0, 0x0, 0, 0, 0, 255, 0);
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

    sendto(sockfd, data, 9,  0, (struct sockaddr*)NULL, sizeof(servaddr));
    recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)NULL, NULL);
    puts(buffer);
    close(sockfd);

    free(data);

    return 0;
}

int interface_listen(){
    // Receiver (Se comporta como Servidor)
    char buffer[BUFFER_SIZE];
    char *message = "Hello Client";
    int listenfd, len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    listenfd = socket(AF_INET, SOCK_DGRAM, 0);        
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET; 
 
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
     
    len = sizeof(cliaddr);
    int n = recvfrom(listenfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr,&len); //receive message from server
    buffer[n] = '\0';
    puts(buffer);
    sendto(listenfd, message, strlen(message), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
    return 0;
}

int main(int argc, char * argv[]){
        // --listen -> Cant be host
        // --port -> Default, must be used for both, i guess?
        // --file (only on sender, its half-duplex)
        // --host (ip) != listenner

        if(argc == 2){  
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
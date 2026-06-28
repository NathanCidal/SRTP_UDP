#ifndef _SRTP_H
#define _SRTP_H

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

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
 |Length (8 Bits)|                 CRC32 (32 Bits) ...             |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |CRC32 (32 Bits)| ############################################### | 
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

 * - Length = 255 - pacote intermediario; o receiver bufferiza e aguarda;
 * - Length < 255 - ultimo pacote do stream; o receiver entrega o buffer completo a aplicação (push).
 * - Length = 0   - edge case: o arquivo e multiplo exato de 255, sinaliza fim de stream sem payload residual.
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

#define BUFFER_SIZE 4096

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
int srtp_send(int sockfd, FILE *file, const struct sockaddr_in *dest_addr, uint8_t window_size, int mode);
int srtp_receive(int sockfd, FILE * file_output, struct sockaddr_in * source_addr, uint8_t window_size, int mode);

// Communication Finish Handshake
int srtp_close(int sockfd, struct sockaddr_in *dest_addr, uint16_t last_seq_count);

#endif
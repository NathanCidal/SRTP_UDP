#ifndef _PARSER_H
#define _PARSER_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/types.h>
#include <unistd.h>

#define LISTEN_P     0
#define HOST_P       1
#define IP_P         2
#define PORT_P       3
#define PORT_VAL_P   4
#define FILE_P       5
#define FILE_NAME_P  6
#define MODE_P       7
#define SAW_P        8
#define GBN_P        9
#define SR_P        10

uint8_t * parser_worker(int argc, char * argv[]);

uint8_t listen_and_send_at_once(uint8_t * parameters);
uint8_t parameters_amount(uint8_t * parameters);
uint8_t file_missing(uint8_t * parameters);
uint8_t port_missing(uint8_t * parameters);
uint8_t incorrect_modes(uint8_t * parameters);
uint8_t parser_error_detector(uint8_t * parameters);

#endif
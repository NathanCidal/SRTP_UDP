#include "parser.h"

// List of Parameters (Param == 0 -> Non Existent)
uint8_t * parser_worker(int argc, char * argv[]){
    uint8_t * parser_params = malloc(sizeof(uint8_t) * 13);

    // Cleans memory to assure proper behaviour
    for(int i = 0; i < 13; i++){
        parser_params[i] = 0;
    }

    if(argc > 11){
        return parser_params; // Zero implies in No Parameters or Too Many Parameters
    }

    for(int i = 0; i < argc; i++){
        if(strcmp(argv[i], "--listen") == 0){
            parser_params[LISTEN_P] = i;
        }

        if(strcmp(argv[i], "--host") == 0){
            parser_params[HOST_P] = i;
        }

        if(strcmp(argv[i], "--port") == 0){
            parser_params[PORT_P] = i;
        }

        if(strcmp(argv[i], "--file") == 0){
            parser_params[FILE_P] = i;
        }

        if(strcmp(argv[i], "--mode") == 0){
            parser_params[MODE_P] = i;
        }

        if(strcmp(argv[i], "saw") == 0){
            parser_params[SAW_P] = i;
        }

        if(strcmp(argv[i], "gbn") == 0){
            parser_params[GBN_P] = i;
        }

        if(strcmp(argv[i], "sr") == 0){
            parser_params[SR_P] = i;
        }

        if(strcmp(argv[i], "--size") == 0){
            parser_params[SIZE_P] = i;
        }
    }

    // Host in Parameters
    uint8_t only_one = 1;

    // Identifies IP Location
    if(parser_params[HOST_P] != 0){
        // The must be no parameters that is HOST_P + 1 (at the moment)
        for(int i = 0; i < 13; i++){
            if(parser_params[i] == (parser_params[HOST_P] + 1)){
                only_one = 0;
                break;
            }
        }

        if(only_one && ((parser_params[HOST_P] + 1) < argc)){
            parser_params[IP_P] = parser_params[HOST_P] + 1;

            if(strlen(argv[parser_params[IP_P]]) >= 2){
                if(strstr(argv[parser_params[IP_P]], "--")){
                        parser_params[IP_P] = 0;
                }
            }
        }
    }

    // Identifies selected PORT for communication
    if(parser_params[PORT_P] != 0){
        only_one = 1;

        // The must be no parameters that is PORT_P + 1 (at the moment)
        for(int i = 0; i < 13; i++){
            if(parser_params[i] == (parser_params[PORT_P] + 1)){
                only_one = 0;
                break;
            }
        }

        if(only_one && ((parser_params[PORT_P] + 1) < argc)){
            parser_params[PORT_VAL_P] = parser_params[PORT_P] + 1;

           if(strlen(argv[parser_params[PORT_VAL_P]]) >= 2){
                if(strstr(argv[parser_params[PORT_VAL_P]], "--")){
                        parser_params[PORT_VAL_P] = 0;
                }
            }
        }
    }

    // Identifies the File Name
    if(parser_params[FILE_P] != 0){
        only_one = 1;

        // The must be no parameters that is FILE_P + 1 (at the moment)
        for(int i = 0; i < 13; i++){
            if(parser_params[i] == (parser_params[FILE_P] + 1)){
                only_one = 0;
                break;
            }
        }

        if(only_one && ((parser_params[FILE_P] + 1) < argc)){
            parser_params[FILE_NAME_P] = parser_params[FILE_P] + 1;

                if(strlen(argv[parser_params[FILE_NAME_P]]) >= 2){
                        if(strstr(argv[parser_params[FILE_NAME_P]], "--")){
                                parser_params[FILE_NAME_P] = 0;
                        }
                }
        }
    }

    // Identifies the Window Size
    if(parser_params[SIZE_P] != 0){
        only_one = 1;

        // The must be no parameters that is SIZE_P + 1 (at the moment)
        for(int i = 0; i < 13; i++){
            if(parser_params[i] == (parser_params[SIZE_P] + 1)){
                only_one = 0;
                break;
            }
        }

        if(only_one && ((parser_params[SIZE_P] + 1) < argc)){
            parser_params[SIZE_VAL_P] = parser_params[SIZE_P] + 1;

                if(strlen(argv[parser_params[SIZE_VAL_P]]) >= 2){
                        if(strstr(argv[parser_params[SIZE_VAL_P]], "--")){
                                parser_params[SIZE_VAL_P] = 0;
                        }
                }
        }
    }

    // There must be only one mode
    if(parser_params[MODE_P] != 0){
        if((parser_params[SAW_P]) && (parser_params[GBN_P] || parser_params[SR_P])){
            for(int i = SAW_P; i < SAW_P + 3; i++) parser_params[i] = 0;
        }

        if((parser_params[GBN_P]) && (parser_params[SAW_P] || parser_params[SR_P])){
            for(int i = SAW_P; i < SAW_P + 3; i++) parser_params[i] = 0;
        }

        if((parser_params[SR_P]) && (parser_params[SAW_P] || parser_params[GBN_P])){
            for(int i = SAW_P; i < SAW_P + 3; i++) parser_params[i] = 0;
        }
    }

    return parser_params;
}

uint8_t listen_and_send_at_once(uint8_t * parameters){
        if(parameters[LISTEN_P] && parameters[HOST_P]){
                printf("! Error ! - Execution cannot be on listen and host mode at same time!\n");
                return 1;
        }
        if((parameters[LISTEN_P] == 0) && (parameters[HOST_P] == 0)){
                printf("! Error ! - Execution must have --listen or --host parameter\n");
                return 1;
        }
        return 0;
}

uint8_t parameters_amount(uint8_t * parameters){
        for(int i = 0; i < 11; i++){
                if(parameters[i] != 0){
                        return 0;
                }
        }

        printf("! Error ! - No Parameters on the application or too many parameters, max of 9\n");
        return 1;
}

uint8_t file_missing(uint8_t * parameters){
        if(parameters[LISTEN_P]){
                if((parameters[FILE_P] != 0) || (parameters[FILE_NAME_P] != 0)){
                        printf("! Error ! - Listen Mode doesnt receive \"--file file_name\" as an input\n");
                        return 1;
                }
        }

        if(parameters[HOST_P]){
                if(parameters[FILE_P] == 0){
                        printf("! Error ! - For host (or sender), there must be a \"--file file_name\" parameters\n");
                        return 1;
                }

                if(parameters[FILE_P] && (parameters[FILE_NAME_P] == 0)){
                        printf("! Error ! - For host (or sender), there must be a \"--file file_name\" parameters, and file_name must not begin with \"--\"\n");
                        return 1;
                }
        }
        return 0;
}

uint8_t port_missing(uint8_t * parameters){
        if(parameters[PORT_P] == 0){
                printf("! Error ! - Missing \"--port\" Parameters. Example of correct input: \"--port 8080\"\n");
                return 1;
        }
        
        if(parameters[PORT_P]){
                if(parameters[PORT_VAL_P] == 0){
                        printf("! Error ! - Missing Port Value or Port Value has \"--\" inside its parameter\n");
                        return 1;
                }
        }
        return 0;
}

uint8_t incorrect_modes(uint8_t * parameters){
        if(parameters[MODE_P]){
                int at_least_one = 0;
                for(int i = SAW_P; i < SAW_P + 3; i++){
                        if(parameters[i] != 0){
                                at_least_one = 1;
                                break;
                        }
                }

                if(at_least_one == 0){
                        printf("! Error ! - Missing Mode Specification or Many Modes selected. Example: \"--mode [saw|gbn|sr]\"\n");
                        return 1;
                }
        }
        return 0;
}

uint8_t missing_size(uint8_t * parameters){
    if(parameters[SIZE_P]){
        if(parameters[SIZE_VAL_P] == 0){
            printf("! Error ! - Missing Window Size! Either insert a window size like: \"--size 4\" or leave empty!\n");
            return 1;
        }
    }
    return 0;
}

uint8_t parser_error_detector(uint8_t * parameters){
        uint8_t error  = parameters_amount(parameters);        // 0 Parameters or Too Many Parameters
                error |= listen_and_send_at_once(parameters); // Both cannot be
                error |= file_missing(parameters);            // Host Only
                error |= port_missing(parameters);            // Generic
                error |= incorrect_modes(parameters);         // Generic
                error |= missing_size(parameters);
        return error;
}
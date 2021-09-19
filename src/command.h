#ifndef COMMAND_H
#define COMMAND_H

#include <stdint.h>

#define COMMAND_TIMEOUT 1000

#define DEFAULT_PORT 1256

#define STATUS_CONNECTED 0x0
#define STATUS_CONNECTING 0x1
#define STATUS_RECORDING 0x2

enum {
    CMD_EMPTY = 0,
    CMD_CONNECT = 1,
    CMD_DISCONNECT = 2,
    CMD_START_RECORDING = 3,
    CMD_STOP_RECORDING = 4,
    CMD_GET_STATUS = 5,
    CMD_SPLIT_RECORDING = 6,
    CMD_QUIT = 7,
    CMD_UPDATE_SONGNAME = 8
};

enum {
    SERVER_MODE_OFF = 0,
    SERVER_MODE_LOCAL = 1,
    SERVER_MODE_ALL = 2
};

typedef struct command {
    uint32_t cmd;
    uint32_t param_size;
    void *param;
}command_t;

int command_start_server(int port, int search_port, int mode);
int command_send_cmd(command_t command, char *addr, int port);
void command_set_new_cmd(command_t command);
void command_get_last_cmd(command_t *command);
void command_send_status_reply(uint32_t status);
int command_recv_status_reply(uint32_t *status);

#endif

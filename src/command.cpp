// command functions for butt
//
// Copyright 2007-2020 by Daniel Noethen.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef WIN32
 #include <winsock2.h>
 #ifndef errno 
  #define errno WSAGetLastError() 
 #endif
 #ifndef EWOULDBLOCK
  #define EWOULDBLOCK WSAEWOULDBLOCK
 #endif
#else
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <unistd.h>
 #include <netinet/in.h> //defines IPPROTO_TCP on BSD
 #include <netdb.h>
 #include <sys/select.h>
 #include <errno.h>
#endif

#ifdef WIN32
 typedef int socklen_t;
#endif

#include "command.h"
#include "sockfuncs.h"

command_t server_cmd;


pthread_t listen_thread;
int listen_sock;
int client_sock;
int conn_sock; 

void *listen_thread_func(void *data)
{
    command_t command;
    int bytes_count;
    struct sockaddr_in cli; 
    socklen_t len; 
    len = sizeof(cli);
    char recv_buf[1024];
    
    server_cmd.cmd = CMD_EMPTY;
    server_cmd.param = NULL;
    server_cmd.param_size = 0;
  
    for (;;) {
        conn_sock = accept(listen_sock, (struct sockaddr*)&cli, &len); 
        sock_nonblock(conn_sock);
        bytes_count = sock_recv(conn_sock, recv_buf, sizeof(recv_buf), COMMAND_TIMEOUT);
        
        if (bytes_count > 0) {
            memcpy((char*)&command, recv_buf, sizeof(command_t));
            if (command.param_size > 0) {
                int expected_bytes = sizeof(command_t)+command.param_size;
                if (bytes_count != expected_bytes) {
                    int remaining_bytes = expected_bytes-bytes_count;
                    bytes_count += sock_recv(conn_sock, recv_buf+bytes_count, remaining_bytes, COMMAND_TIMEOUT);
                    if (bytes_count != expected_bytes) {
                        sock_close(conn_sock);
                        continue; // Still not all bytes received -> dismiss this command
                    }
                }
                command.param = (void*)malloc(command.param_size);
                memcpy((char*)command.param, recv_buf+sizeof(command_t), command.param_size);
            }
            
            command_set_new_cmd(command);
        }
    }

    return NULL;
}

int command_start_server(int port, int search_port, int mode)
{

#ifdef WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2),&wsa);
#endif

    struct sockaddr_in servaddr;
  
    listen_sock = socket(AF_INET, SOCK_STREAM, 0); 
    if(listen_sock == -1)
          return SOCK_ERR_CREATE;
    memset(&servaddr, 0, sizeof(servaddr)); 
  
    servaddr.sin_family = AF_INET; 

    if (mode == SERVER_MODE_LOCAL)
        servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
    else // SERVER_MODE_ALL (accessable from the network)
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 

    int bind_succeeded = 0;
    int p;
    for (p = port; p < port+10; p++)
    {
        servaddr.sin_port = htons(p);
      
        if (bind(listen_sock, (struct sockaddr*)&servaddr, sizeof(servaddr)) == 0)
        {
            bind_succeeded = 1;
            break;
        }
        
        if (search_port == 0)
            break;
    }
    
    if (bind_succeeded == 0)
        return SOCK_ERR_BIND;

    if ((listen(listen_sock, 1)) != 0) 
          return SOCK_ERR_LISTEN;

    pthread_create(&listen_thread, NULL, listen_thread_func, NULL);

    return p;
}


int command_send_cmd(command_t command, char *addr, int port)
{
    char send_buf[1024];
    client_sock = sock_connect(addr, port, COMMAND_TIMEOUT);
    if (client_sock < 0)
        return client_sock;

    memcpy(send_buf, &command, sizeof(command_t));
    memcpy(send_buf+sizeof(command_t), (char*)command.param, command.param_size);

    sock_send(client_sock, send_buf, sizeof(command_t)+command.param_size, COMMAND_TIMEOUT);
    

    if (command.cmd != CMD_GET_STATUS) 
        sock_close(client_sock);

    return 0;
}

void command_get_last_cmd(command_t *command)
{
    memcpy(command, &server_cmd, sizeof(command_t));
    
    server_cmd.cmd = CMD_EMPTY;
    server_cmd.param_size = 0;
    server_cmd.param = NULL;
}

void command_set_new_cmd(command_t command)
{
    server_cmd = command;
}


void command_send_status_reply(uint32_t status)
{
    sock_send(conn_sock, (char*)&status, sizeof(uint32_t), COMMAND_TIMEOUT);
}

int command_recv_status_reply(uint32_t *status)
{
    return sock_recv(client_sock, (char*)status, sizeof(uint32_t), COMMAND_TIMEOUT);
}


#include <stdio.h>
#include <string.h>

#include "config.h"
#include "update.h"
#include "sockfuncs.h"

char new_version[16];

int update_check_for_new_version(void)
{
    int ret;
    int socket;
    char request[1024];
    char response[1024];
 
    memset(new_version, 0, sizeof(new_version));
    
    socket = sock_connect("danielnoethen.de", 80, CONN_TIMEOUT);
    if (socket < 0)
        return UPDATE_SOCKET_ERROR;
    
    
    snprintf(request, sizeof(request), "%s",
             "GET /latest_butt HTTP/1.0\r\n"
             "Host: danielnoethen.de\r\n"
             "Connection: close\r\n\r\n"
             );
    
    ret = sock_send(socket, request, (int)strlen(request), SEND_TIMEOUT);
    if (ret == SOCK_TIMEOUT)
        ret = UPDATE_SOCKET_ERROR;
    
    ret = sock_recv(socket, response, sizeof(response), 5*RECV_TIMEOUT);
    
    if (ret <= 0)
        return UPDATE_SOCKET_ERROR;
    
    response[ret] = '\0';
    
    char *p = strstr(response, "version: ");
    if (p == NULL)
        return UPDATE_ILLEGAL_ANSWER;
    
    p += strlen("version: ");

    if (p[strlen(p)-1] == '\n') {
        p[strlen(p)-1] = '\0';
    }
    
    if (strstr(p, VERSION) != NULL)
        return UPDATE_UP_TO_DATE;
    
    snprintf(new_version, sizeof(new_version)-1, "%s", p);
    return UPDATE_NEW_VERSION;
}

char *update_get_version(void)
{
    return new_version;
}

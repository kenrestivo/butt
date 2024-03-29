// icecast functions for butt
//
// Copyright 2007-2018 by Daniel Noethen.
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

#ifdef WIN32
#include <winsock2.h>
#define usleep(us) Sleep(us/1000)
//#define close(s) closesocket(s)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h> //defines IPPROTO_TCP on BSD
#include <netdb.h>
#endif

#include <errno.h>

#include "config.h"
#include "gettext.h"
#include "cfg.h"
#include "butt.h"
#include "util.h"
#include "timer.h"
#include "icecast.h"
#include "strfuncs.h"
#include "sockfuncs.h"
#include "parseconfig.h"
#include "fl_funcs.h"
#include "flgui.h"

#ifdef HAVE_LIBSSL
 #include "tls.h"
#endif

#ifdef HAVE_LIBSSL
 tls_t stream_tls;
#endif

int ic_recv(char *buf, int buf_len);

int ic_connect(void)
{
    int ret;
    int retval;
    int tries = 2;
    int opus_supported = 0;
    char auth[150];
    char b64_auth[200];
    char recv_buf[1000];
    char send_buf[2048];
    char msg[256];
    char *http_retval;
    static bool error_printed = 0;
    
    for (int try_cnt = 0; try_cnt < tries; try_cnt++)
    {

        stream_socket = sock_connect(cfg.srv[cfg.selected_srv]->addr,
                cfg.srv[cfg.selected_srv]->port, CONN_TIMEOUT);

        if (stream_socket < 0)
        {
            switch (stream_socket)
            {
                case SOCK_ERR_CREATE:
                    if(!error_printed)
                    {
                        print_info(_("\nconnect: Could not create network socket"), 1);
                        error_printed = 1;
                    }
                    if (cfg.main.force_reconnecting == 1)
                        ret = IC_RETRY;
                    else
                        ret = IC_ABORT;
                    break;
                case SOCK_ERR_RESOLVE:
                    if(!error_printed)
                    {
                        print_info(_("\nconnect: Error resolving server address"), 1);
                        error_printed = 1;
                    }
                    ret = IC_RETRY;
                    break;
                case SOCK_TIMEOUT:
                case SOCK_INVALID:
                    ret = IC_RETRY;
                    break;
                default:
                    if (cfg.main.force_reconnecting == 1)
                        ret = IC_RETRY;
                    else
                        ret = IC_ABORT;
                    break;
            }

            return ret;
        }

#ifdef HAVE_LIBSSL
        if (cfg.srv[cfg.selected_srv]->tls == 1)
        {
            stream_tls.host = cfg.srv[cfg.selected_srv]->addr;
            stream_tls.socket = stream_socket;
            stream_tls.cert_file = cfg.tls.cert_file;
            stream_tls.cert_dir = cfg.tls.cert_dir;

            if ((ret = tls_setup(&stream_tls)) != TLS_OK)
            {
                // Check if the user wants to ignore a certificate verification error
                int ignore_verfication_error;
                if ( (cfg.srv[cfg.selected_srv]->cert_hash != NULL) && (!strcmp(stream_tls.sha256, cfg.srv[cfg.selected_srv]->cert_hash)) )
                    ignore_verfication_error = 1;
                else
                    ignore_verfication_error = 0;
                
                if (ret == TLS_TIMEOUT)
                {
                    print_info(_("\nconnect: SSL connection timed out. Trying again..."), 1);
                    ic_disconnect();
                    return IC_RETRY;
                }
                else if ( (ret == TLS_CHECK_CERT) || (ret == TLS_CHECK_HOST) )
                {
                    if (ignore_verfication_error == 0)
                    {
                        snprintf(msg, sizeof(msg),
                                 _("SSL/TLS certificate verification failed\nReason: %s\n\n"
                                 "Do you still want to trust this certificate?\n"
                                 "Trusting will be permanent and can be revoked\n"
                                 "in the server settings."), stream_tls.last_err);
                        
                        ask_user_set_msg(msg);
                        ask_user_set_hash(stream_tls.sha256);
                        ic_disconnect();
                        return IC_ASK;
                    }
                }
                else
                {
                    if(!error_printed)
                    {
                        snprintf(msg, sizeof(msg), _("\nconnect: SSL connection failed\n"
                                 "Reason: %s"), stream_tls.last_err);
                        print_info(msg, 1);
                        error_printed = 1;
                    }
                    ic_disconnect();
                    if (cfg.main.force_reconnecting == 1)
                        return IC_RETRY;
                    else
                        return IC_ABORT;
                }
            }
        }
#endif // HAVE_LIBSSL
        if (try_cnt == 0)
        {
            // Try PUT method first. Supported since icecast 2.4.0
            if(cfg.srv[cfg.selected_srv]->mount[0] != '/')
                snprintf(send_buf, sizeof(send_buf), "PUT /%s HTTP/1.1\r\n",
                        cfg.srv[cfg.selected_srv]->mount);
            else
                snprintf(send_buf, sizeof(send_buf), "PUT %s HTTP/1.1\r\n",
                        cfg.srv[cfg.selected_srv]->mount);

            opus_supported = 1;
        }
        else
        {
            if (cfg.srv[cfg.selected_srv]->mount[0] != '/')
                snprintf(send_buf, sizeof(send_buf), "SOURCE /%s HTTP/1.0\r\n",
                        cfg.srv[cfg.selected_srv]->mount);
            else
                snprintf(send_buf, sizeof(send_buf), "SOURCE %s HTTP/1.0\r\n",
                        cfg.srv[cfg.selected_srv]->mount);
        }

        snprintf(auth, sizeof(auth), "%s:%s", cfg.srv[cfg.selected_srv]->usr, cfg.srv[cfg.selected_srv]->pwd);
        snprintf(b64_auth, sizeof(b64_auth), "%s", util_base64_enc(auth));
        snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "Authorization: Basic %s\r\n", b64_auth);
        

        // Make butt compatible to proxies/load balancers. Thanks to boyska
        if(cfg.srv[cfg.selected_srv]->port == 80)
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "Host: %s\r\n", cfg.srv[cfg.selected_srv]->addr);
        else
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "Host: %s:%d\r\n", cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port);
       
        //ic_send(send_buf, (int)strlen(send_buf));

        snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "User-Agent: %s\r\n", PACKAGE_STRING);
        //ic_send(send_buf, (int)strlen(send_buf));

        if(!strcmp(cfg.audio.codec, "mp3"))
            snprintf(send_buf+strlen(send_buf),  sizeof(send_buf)-strlen(send_buf), "Content-Type: audio/mpeg\r\n");
        else if(!strcmp(cfg.audio.codec, "aac"))
            snprintf(send_buf+strlen(send_buf),  sizeof(send_buf)-strlen(send_buf), "Content-Type: audio/aac\r\n");
        else
            snprintf(send_buf+strlen(send_buf),  sizeof(send_buf)-strlen(send_buf), "Content-Type: audio/ogg\r\n");


        if(cfg.main.num_of_icy > 0)
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "ice-name: %s\r\n", cfg.icy[cfg.selected_icy]->name);
        else
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "ice-name: no name\r\n");

        if(cfg.main.num_of_icy > 0)
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "ice-public: %s\r\n", cfg.icy[cfg.selected_icy]->pub);
        else
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "ice-public: 0\r\n");

        
        if(cfg.main.num_of_icy > 0)
        {
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "ice-url: %s\r\n", cfg.icy[cfg.selected_icy]->url);
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "ice-genre: %s\r\n", cfg.icy[cfg.selected_icy]->genre);
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "ice-description: %s\r\n", cfg.icy[cfg.selected_icy]->desc);
        }


        // Send audio settings
        if(!strcmp(cfg.audio.codec, "flac"))
        { // Do not send bitrate information if flac is used
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf),
                     "ice-audio-info: "
                     "ice-channels=%d;"
                     "ice-samplerate=%d"
                     "\r\n",
                     cfg.audio.channel,
                     strcmp(cfg.audio.codec, "opus") == 0 ? 48000 : cfg.audio.samplerate);
        }
        else
        {
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf),
                     "ice-audio-info: "
                     "ice-bitrate=%d;"
                     "ice-channels=%d;"
                     "ice-samplerate=%d"
                     "\r\n",
                     cfg.audio.bitrate,
                     cfg.audio.channel,
                     strcmp(cfg.audio.codec, "opus") == 0 ? 48000 : cfg.audio.samplerate);
        }
        

        //ic_send(send_buf, (int)strlen(send_buf));


        if (try_cnt == 0) // PUT
        {
            snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "Expect: 100-continue\r\n");
           // ic_send(send_buf, (int)strlen(send_buf));
        }

        snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "\r\n");
        
        ic_send(send_buf, (int)strlen(send_buf));

        ret = ic_recv(recv_buf, sizeof(recv_buf));
        
        //MARK: DEBUG
        //sprintf(recv_buf, "FOOHTTP/1.1 100 Continue\nContent-Length: 0\r\n\r\n");
        //print_info(recv_buf, 1);
        

        if (ret == 0)
        {
            if (try_cnt == 0)
            {
                ic_disconnect();
                continue; //try SOURCE method if PUT method did not work
            }
            else
            {
                usleep(100*1000);
                ic_disconnect();
                return IC_RETRY;
            }
        }
        if (ret == SOCK_TIMEOUT)
        {
            print_info(_("\nconnect: connection timed out. Trying again..."), 1);
            usleep(100000);
            ic_disconnect();
            return IC_RETRY;
        }
        if (ret < 0)
        {
            //print_info("\nconnect: error while receiving server response\nThe server might require SSL/TLS", 1);
            ic_disconnect();
            return IC_RETRY;
        }

        char* temp = strdup(recv_buf);
        //We need to extract the HTTP return value from the HTTP response
        //to see if the login was successfull (HTTP/1.0 200 OK)
        http_retval = strchr(temp, ' ');
        if(http_retval == NULL)
        {
            usleep(100000);
            ic_disconnect();
            free(temp);
            return IC_RETRY;
        }
        //point to the beginning of the HTTP return value
        http_retval++;
        http_retval[3] = '\0';
        
        retval = atoi(http_retval);
        free(temp);
        
        // Workaround for liquidsoap and icecast-kh
        if (retval == 100) {
            if (strstr(recv_buf, " 401 ") != NULL) { // Wrong password
                retval = 401;
            }
            
            if (strstr(recv_buf, " 403 ") != NULL) { // Mounpoint in use (Needed since kh14)
                retval = 403;
            }
        }
 

        if (retval != 200 && retval != 100)
        {
            switch (retval)
            {
                case 401:
                    if(!error_printed)
                    {
                        print_info(_("\nconnect: invalid user/password!\n"), 1);
                        error_printed = 1;
                    }
                    ic_disconnect();
                    if (cfg.main.force_reconnecting == 1)
                        return IC_RETRY;
                    else
                        return IC_ABORT;
                    break;
                case 403:   //mountpoint already in use
                    usleep(100000);
                    ic_disconnect();
                    return IC_RETRY;
                    break;
                case 404:
                    if (try_cnt == 0)
                    {
                        ic_disconnect();// This brings compatibility to airtime server. Because they don't understand the PUT method they answer with an 404
                        opus_supported = 1; // Airtimes supports Opus
                        usleep(100000);
                        continue;       // Let's try the SOURCE method then...
                    }
                    print_info(_("\nconnect: server answered with 404!\n"), 1);

                    ic_disconnect();
                    return IC_ABORT;
                    break;
                default:
                    if(!error_printed)
                    {
                        snprintf(msg, sizeof(msg), _("\nconnect: server answered with %d!\n"), retval);
                        print_info(msg, 1);
                        error_printed = 1;
                    }
                    ic_disconnect();
                    if (cfg.main.force_reconnecting == 1)
                        return IC_RETRY;
                    else
                        return IC_ABORT;
            }
        }

        // At this point the connection has been established and encoded data
        // can be send to the server

        // In case Opus is selected we need to verifiy if it is supported by
        // the server
        if (!strcmp(cfg.audio.codec, "opus"))
        {
            if (opus_supported == 1) // The server has at least version 2.4.0 (PUT method worked) or it is an airtime server -> opus is supported.
            {
                break;
            }
            else
            {
                print_info(_("\nERROR: Opus is not supported by your\nIcecast server (>=1.4.0 required)!\n"), 1);
                ic_disconnect();
                return IC_ABORT;
            }
        }

        break;
    }
    
    connected = 1;
    error_printed = 0;

    timer_init(&stream_timer, 1);       //starts the "online" timer

    return IC_OK;
}


int ic_send(char *buf, int buf_len)
{
    int ret;
    if (cfg.srv[cfg.selected_srv]->tls == 1)
    {
#ifdef HAVE_LIBSSL
        ret = tls_send(&stream_tls, buf, buf_len, SEND_TIMEOUT);
        if (ret == TLS_SENDERR)
#endif
            ret = -1;
    }
    else
    {
        ret = sock_send(stream_socket, buf, buf_len, SEND_TIMEOUT);
        if (ret == SOCK_TIMEOUT) {
            ret = -1;
        }
    }
   
    return ret;
}

int ic_recv(char *buf, int buf_len) 
{
    int ret;
    if (cfg.srv[cfg.selected_srv]->tls == 1) 
    {
#ifdef HAVE_LIBSSL
        ret = tls_recv(&stream_tls, buf, buf_len, 5*RECV_TIMEOUT);
        if (ret != TLS_TIMEOUT)
            return ret;
        else
#endif
            return SOCK_TIMEOUT;
    }
    else
        return sock_recv(stream_socket, buf, buf_len, 5*RECV_TIMEOUT);
}

int ic_update_song(char *song_name)
{
    int ret;
    int web_socket;
    char send_buf[1024];
    char auth[150];
    char *song_buf;
    char *mount;
#ifdef HAVE_LIBSSL
    tls_t web_tls;
#endif

    web_socket = sock_connect(cfg.srv[cfg.selected_srv]->addr,
            cfg.srv[cfg.selected_srv]->port, CONN_TIMEOUT);

    if(web_socket < 0)
    {
        switch(web_socket)
        {
            case SOCK_ERR_CREATE:
                print_info(_("\nupdate_song: could not create network socket"), 1);
                ret = IC_ABORT;
                break;
            case SOCK_ERR_RESOLVE:
                print_info(_("\nupdate_song: error resolving server address"), 1);
                ret = IC_ABORT;
                break;
            case SOCK_TIMEOUT:
            case SOCK_INVALID:
                ret = IC_RETRY;
                break;
            default:
                ret = IC_ABORT;
        }

        return ret;
    }
    
#ifdef HAVE_LIBSSL
    if (cfg.srv[cfg.selected_srv]->tls == 1)
    {
        web_tls.host = cfg.srv[cfg.selected_srv]->addr;
        web_tls.socket = web_socket;
        web_tls.cert_file = cfg.tls.cert_file;
        web_tls.cert_dir = cfg.tls.cert_dir;
        web_tls.skip_verification = 0;
        
        if ( (cfg.srv[cfg.selected_srv]->cert_hash != NULL) && (!strcmp(stream_tls.sha256, cfg.srv[cfg.selected_srv]->cert_hash)) )
            web_tls.skip_verification = 1;
        
        if ((ret = tls_setup(&web_tls)) != TLS_OK)
        {
            sock_close(web_socket);
            return IC_ABORT;
        }
    }
#endif

    song_buf = strdup(song_name);

    strrpl(&song_buf, (char*)" ", (char*)"%20", MODE_ALL);
    strrpl(&song_buf, (char*)"&", (char*)"%26", MODE_ALL);

    mount = (char*)malloc(strlen(cfg.srv[cfg.selected_srv]->mount)+2);

    if(cfg.srv[cfg.selected_srv]->mount[0] != '/')
        sprintf(mount, "/%s", cfg.srv[cfg.selected_srv]->mount);
    else
        strcpy(mount, cfg.srv[cfg.selected_srv]->mount);

    snprintf(auth, sizeof(auth), "%s:%s", cfg.srv[cfg.selected_srv]->usr, cfg.srv[cfg.selected_srv]->pwd);

    if (cfg.main.ic_charset != NULL) {
        snprintf(send_buf, sizeof(send_buf), "GET /admin/metadata?mode=updinfo&mount=%s&charset=%s&song=%s "
                "HTTP/1.0\r\n"
                "User-Agent: %s\r\n"
                "Authorization: Basic %s\r\n",
                mount, cfg.main.ic_charset, song_buf, PACKAGE_STRING, util_base64_enc(auth));
    }
    else {
        snprintf(send_buf, sizeof(send_buf), "GET /admin/metadata?mode=updinfo&mount=%s&song=%s "
                "HTTP/1.0\r\n"
                "User-Agent: %s\r\n"
                "Authorization: Basic %s\r\n",
                mount, song_buf, PACKAGE_STRING, util_base64_enc(auth));

    }


    if( cfg.srv[cfg.selected_srv]->port == 80)
        snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "Host: %s\r\n\r\n", cfg.srv[cfg.selected_srv]->addr);
    else
        snprintf(send_buf+strlen(send_buf), sizeof(send_buf)-strlen(send_buf), "Host: %s:%d\r\n\r\n", cfg.srv[cfg.selected_srv]->addr, cfg.srv[cfg.selected_srv]->port);

    if (cfg.srv[cfg.selected_srv]->tls == 1)
    {
#ifdef HAVE_LIBSSL
        ret = tls_send(&web_tls, send_buf, (int)strlen(send_buf), SEND_TIMEOUT);
        tls_close(&web_tls);
#endif
    }
    else
    {
        sock_send(web_socket, send_buf, (int)strlen(send_buf), SEND_TIMEOUT);
    }
    
    sock_close(web_socket);

    free(song_buf);
    free(mount);

    return IC_OK;
}


void ic_disconnect(void)
{
#ifdef HAVE_LIBSSL
    if (cfg.srv[cfg.selected_srv]->tls == 1)
        tls_close(&stream_tls);
#endif
        
    sock_close(stream_socket);
}


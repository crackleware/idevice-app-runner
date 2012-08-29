/**
 * run app on iDevice using com.apple.debugserver - by <predrg@gmail.com>
 *
 * based on:
 *   ideviceinstaller - Copyright (C) 2010 Nikias Bassen <nikias@gmx.li>
 *   remote.c - from gdb
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more profile.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 
 * USA
 */

/*
  build me with:
  $ gcc -g -pthread idevice-app-runner.c -o idevice-app-runner /usr/lib/libimobiledevice.so
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

char *uuid = NULL;
char *apppath = NULL;

int run_mode = 0;

static void print_usage(int argc, char **argv)
{
    char *name = NULL;

    name = strrchr(argv[0], '/');
    printf("Usage: %s OPTIONS\n", (name ? name + 1 : argv[0]));
    printf("Run (debug) apps on an iDevice.\n\n");
    printf
        ("  -U, --uuid UUID\tTarget specific device by its 40-digit device UUID.\n"
         "  -r, --run PATH\tRun (debug) app specified by on-device path (use ideviceinstaller -l -o xml to find it).\n"
         "  -h, --help\t\tprints usage information\n"
         "  -d, --debug\t\tenable communication debugging\n" "\n");
}

static void parse_opts(int argc, char **argv)
{
    static struct option longopts[] = {
        {"help", 0, NULL, 'h'},
        {"uuid", 1, NULL, 'U'},
        {"run", 1, NULL, 'r'},
        {"debug", 0, NULL, 'd'},
        {NULL, 0, NULL, 0}
    };
    int c;

    while (1) {
        c = getopt_long(argc, argv, "hU:r:d", longopts,
                        (int *) 0);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            print_usage(argc, argv);
            exit(0);
        case 'U':
            if (strlen(optarg) != 40) {
                printf("%s: invalid UUID specified (length != 40)\n",
                       argv[0]);
                print_usage(argc, argv);
                exit(2);
            }
            uuid = strdup(optarg);
            break;
        case 'r':
            run_mode = 1;
            apppath = strdup(optarg);
            break;
        case 'd':
            idevice_set_debug_level(1);
            break;
        default:
            print_usage(argc, argv);
            exit(2);
        }
    }

    if (optind <= 1 || (argc - optind > 0)) {
        print_usage(argc, argv);
        exit(2);
    }
}

//#define WITH_DEBUG

char tohex(int x)
{
    static char* hexchars = "0123456789ABCDEF";
    return hexchars[x];
}

unsigned int fromhex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'a' && c <= 'f')
        return 10 + c - 'a';
    else if (c >= 'A' && c <= 'F')
        return 10 + c - 'A';
}

void send_str(char* buf, idevice_connection_t connection)
{
    int bytes = 0;
    idevice_connection_send(connection, buf, strlen(buf), (uint32_t*)&bytes);
#ifdef WITH_DEBUG
    printf("send: bytes=%d (%s)\n", bytes, buf);
#endif
}

void recv_pkt(idevice_connection_t connection)
{
    int bytes = 0;
    char buf[16*1024];
    idevice_connection_receive_timeout(connection, buf, sizeof(buf)-1, &bytes, 1000);
#ifdef WITH_DEBUG
    printf("recv: bytes=%d\n", bytes);
#endif
    if (bytes >= 0)
        buf[bytes] = 0x00;
#ifdef WITH_DEBUG
    printf("d='%s'\n", buf);
#endif
    if (bytes > 0 && buf[0] == '$') {
        send_str("+", connection);
        if (bytes > 1 && buf[1] == 'O') {
            char* c = buf+2;
            char* bufend = buf+strlen(buf);
            char buf3[16*1024];
            int i = 0;
            while (*c != '#' && c < bufend)
                buf3[i++] = fromhex(*c++) << 4 | fromhex(*c++);
            buf3[i] = 0x00;
#ifdef WITH_DEBUG
            printf("recv: data='%s'\n", buf3);
#else
            printf("%s", buf3);
#endif
            fflush(stderr);
            fflush(stdout);
        }
    }
}

void send_pkt(char* buf, idevice_connection_t connection)
{
    int i;
    unsigned char csum = 0;
    char *buf2 = malloc (32*1024);
    int cnt = strlen (buf);
    char *p;

    /* Copy the packet into buffer BUF2, encapsulating it
       and giving it a checksum.  */

    p = buf2;
    *p++ = '$';

    for (i = 0; i < cnt; i++)
    {
        csum += buf[i];
        *p++ = buf[i];
    }
    *p++ = '#';
    *p++ = tohex ((csum >> 4) & 0xf);
    *p++ = tohex (csum & 0xf);

    *p = '\0';

    int bytes = 0;
    idevice_connection_send(connection, buf2, strlen(buf2), (uint32_t*)&bytes);
#ifdef WITH_DEBUG
    printf("send: bytes=%d (%s)\n", bytes, buf);
#endif
    free(buf2);

    recv_pkt(connection);
}

int main(int argc, char **argv)
{
    idevice_t phone = NULL;
    lockdownd_client_t client = NULL;
    uint16_t port = 0;
    int res = 0;

    parse_opts(argc, argv);

    argc -= optind;
    argv += optind;

    if (!apppath) {
        fprintf(stderr, "App path required.\n");
        return -1;
    }

    if (IDEVICE_E_SUCCESS != idevice_new(&phone, uuid)) {
        fprintf(stderr, "No iPhone found, is it plugged in?\n");
        return -1;
    }

    if (LOCKDOWN_E_SUCCESS != lockdownd_client_new_with_handshake(phone, &client, "idevice-app-runner")) {
        fprintf(stderr, "Could not connect to lockdownd. Exiting.\n");
        goto leave_cleanup;
    }

    uint16_t port2 = 0;
    if ((lockdownd_start_service
         (client, "com.apple.debugserver",
          &port2) != LOCKDOWN_E_SUCCESS) || !port2) {
        fprintf(stderr,
                "Could not start com.apple.debugserver!\n");
        goto leave_cleanup;
    }

    idevice_connection_t connection = NULL;
    if (idevice_connect(phone, port2, &connection) != IDEVICE_E_SUCCESS) {
        fprintf(stderr, "idevice_connect failed!\n");
        goto leave_cleanup;
    }

    /* send_str("+", connection); */

    char* cmds[] = {
        "will be replaced with hex encoding of apppath",
        "Hc0",
        "c",

        NULL,
    };

    cmds[0] = malloc(2000);
    char* p = cmds[0];
    sprintf(p, "A%d,0,", strlen(apppath)*2/* +4 */);
    p += strlen(p);
    char* q = apppath;
    while (*q) {
        *p++ = tohex(*q >> 4);
        *p++ = tohex(*q & 0xf);
        q++;
    }
    /* sprintf(p, "%02x%02x", '/', '.'); */

    char** cmd = cmds;
    while (*cmd) {
        printf("'%s'\n", *cmd);
        sleep(1);
        send_pkt(*cmd, connection);
        recv_pkt(connection);
        recv_pkt(connection);
        cmd++;
    }

    while (1) {
        recv_pkt(connection);
        /* sleep(1); */
    }

    printf("enter to exit..."); getchar();

    if (client) {
        /* not needed anymore */
        lockdownd_client_free(client);
        client = NULL;
    }

    do_wait_when_needed();

leave_cleanup:
    if (client) {
        lockdownd_client_free(client);
    }
    idevice_free(phone);

    if (uuid) {
        free(uuid);
    }
    if (apppath) {
        free(apppath);
    }

    return res;
}

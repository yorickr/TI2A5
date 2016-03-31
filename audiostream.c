//
// Created by Gilian Joosen on 10/03/16.
//

#include "audiostream.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <io.h>

#include <dev/debug.h>
#ifdef ETHERNUT2
#include <dev/lanc111.h>
#else
#include <dev/nicrtl.h>
#endif
#include <dev/vs1001k.h>

#include <sys/version.h>
#include <sys/confnet.h>
#include <sys/heap.h>
#include <sys/bankmem.h>
#include <sys/thread.h>
#include <sys/timer.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <netinet/tcp.h>

#include <pro/dhcp.h>

/*
 * Determine the compiler.
 */
#if defined(__IMAGECRAFT__)
#define CC_STRING   "ICCAVR"
#elif defined(__GNUC__)
#define CC_STRING   "AVRGCC"
#else
#define CC_STRING   "Compiler unknown"
#endif


/*!
 * \example mnut03/mnut03.c
 *
 * Medianut Tutorial - Part 3.
 */

/*!
 * \brief Device for debug output.
 */
#define DBG_DEVICE devDebug0

/*!
 * \brief Device name for debug output.
 */
#define DBG_DEVNAME "uart0"

/*!
 * \brief Baudrate for debug output.
 */
#define DBG_BAUDRATE 115200

/*!
 * \brief Unique MAC address of the Ethernut Board.
 */
#define MY_MAC { 0x00, 0x06, 0x98, 0x10, 0x01, 0x10 }
/*!
 * \brief Unique IP address of the Ethernut Board.
 *
 * Ignored if DHCP is used.
 */
#define MY_IPADDR "192.168.137.100"

/*!
 * \brief IP network mask of the Ethernut Board.
 *
 * Ignored if DHCP is used.
 */
#define MY_IPMASK "255.255.255.0"

/*!
 * \brief Gateway IP address for the Ethernut Board.
 *
 * Ignored if DHCP is used.
 */
#define MY_IPGATE "192.168.137.1"

/*!
 * \brief IP address of the radio station.
 */
//#define RADIO_IPADDR "145.58.53.152"
//#define RADIO_IPADDR "64.236.34.196"

/*!
 * \brief Port number of the radio station.
 */
//#define RADIO_PORT 80

/*!
 * \brief URL of the radio station.
 */
//#define RADIO_URL "/3fm-bb-mp3"
//#define RADIO_URL "/stream/1020"

/*!
 * \brief Size of the header line buffer.
 */
#define MAX_HEADERLINE 512

/*!
 * \brief TCP buffer size.
 */
#define TCPIP_BUFSIZ 8760

/*!
 * \brief Maximum segment size.
 *
 * Choose 536 up to 1460. Note, that segment sizes above 536 may result in
 * fragmented packets. Remember, that Ethernut doesn't support TCP
 * fragmentation.
 */
#define TCPIP_MSS 1460

/*
 * Socket receive timeout.
 */
#define TCPIP_READTIMEOUT 3000


/*!
* \brief Configure Ethernut LAN interface.
*
* If the EEPROM contains a valid network configuration, then it will
* be used. Otherwise DHCP is queried using a hard coded MAC address
* (global macro MY_MAC). If there is no DHCP server available, then
* the hard coded IP settings will be used (global macros MY_IPADDR,
* MY_IPMASK and MY_IPGATE).
*
* \param devname Symbolic name of the network device.
*/
int ConfigureLan(char *devname) {
    /*
     * Calling DHCP without MAC address assumes, that we have a valid
     * configuration in EEPROM.
     */
    puts("configure devname");
    printf("Configure %s...", devname);
    if (NutDhcpIfConfig(devname, NULL, 60000))
        {
            u_char mac[6] = MY_MAC;
            puts("Failed to get IP from dhcp");
            /*
             * DHCP failed. Either because the EEPROM contained no valid
             * MAC address or because we can't contact DHCP. We try again
             * with our hard coded MAC address.
             */
            puts("hard coded MAC...");
            if (NutDhcpIfConfig(devname, mac, 60000)) {
                u_long ip_addr = inet_addr(MY_IPADDR);
                u_long ip_mask = inet_addr(MY_IPMASK);
                u_long ip_gate = inet_addr(MY_IPGATE);

                /*
                 * Network configuration failed again. Give up DHCP and
                 * try the hard coded IP configuration.
                 */
                puts("hard coded IP...");
                if (NutNetIfConfig(devname, mac, ip_addr, ip_mask)) {

                    /*
                     * If even this one fails, then something is completely
                     * wrong. Return an error.
                     */
                    puts("Error: No LAN device");
                    return -1;
                }
                if (ip_gate) {

                    /*
                     * Without DHCP we had to set the default gateway manually.
                     */
                    puts("hard coded gate...");
                    if (NutIpRouteAdd(0, 0, ip_gate, &DEV_ETHER) == 0) {
                        puts("Error: Can't set gateway");
                        return -1;
                    }
                }
            }
        }
        else{
            puts("Got IP from DHCP");
        }

        /*
         * Display the result of our LAN configuration.
         */
        puts("OK");
        printf("MAC : %02X-%02X-%02X-%02X-%02X-%02X\n", confnet.cdn_mac[0], confnet.cdn_mac[1],
               confnet.cdn_mac[2], confnet.cdn_mac[3], confnet.cdn_mac[4], confnet.cdn_mac[5]);
        printf("IP  : %s\n", inet_ntoa(confnet.cdn_ip_addr));
        printf("Mask: %s\n", inet_ntoa(confnet.cdn_ip_mask));
        printf("Gate: %s\n\n", inet_ntoa(confnet.cdn_gateway));

        return 0;
    }

/*!
 * \brief Connect to a radio station.
 *
 * \param sock TCP socket for this connection.
 * \param ip   IP address of the server to connect.
 * \param port Port number of the server to connect.
 *
 * \return Stream pointer of the established connection on success.
 *         Otherwise 0 is returned.
 */
    FILE *ConnectStation(TCPSOCKET *sock, u_long ip, u_short port, u_long *metaint, RADIO_STREAM rStream) {
        int rc;
        FILE *stream;
        u_char *line;
        u_char *cp;

        /*
         * Connect the TCP server.
         */
//        printf("radio ip: %d", rStream.ip)
        printf("Connecting %s:%d...", rStream.radio_ip, rStream.radio_port);
        if ((rc = NutTcpConnect(sock, inet_addr(rStream.radio_ip), rStream.radio_port))) {
            printf("Error: Connect failed with %d\n", NutTcpError(sock));
            return 0;
        }
        puts("OK");

        if ((stream = _fdopen((int) sock, "r+b")) == 0) {
            puts("Error: Can't create stream");
            return 0;
        }

        /*
         * Send the HTTP request.
         */
        printf("GET %s HTTP/1.0\n\n", rStream.radio_url);
        fprintf(stream, "GET %s HTTP/1.0\r\n", rStream.radio_url);
        fprintf(stream, "Host: %s\r\n", rStream.radio_ip);
        fprintf(stream, "User-Agent: Ethernut\r\n");
        fprintf(stream, "Accept: */*\r\n");
        fprintf(stream, "Icy-MetaData: 1\r\n");
        fprintf(stream, "Connection: close\r\n");
        fputs("\r\n", stream);
        fflush(stream);

        /*
         * Receive the HTTP header.
         */
        line = malloc(MAX_HEADERLINE);
        while (fgets(line, MAX_HEADERLINE, stream)) {

            /*
             * Chop off the carriage return at the end of the line. If none
             * was found, then this line was probably too large for our buffer.
             */
            cp = strchr(line, '\r');
            if (cp == 0) {
                puts("Warning: Input buffer overflow");
                continue;
            }
            *cp = 0;

            /*
             * The header is terminated by an empty line.
             */
            if (*line == 0) {
                break;
            }
            if (strncmp(line, "icy-metaint:", 12) == 0) {
                *metaint = atol(line + 12);
            }
            printf("%s\n", line);
        }
        putchar('\n');

        free(line);

        return stream;
    }

/*
 * Process embedded meta data.
 */
    int ProcessMetaData(FILE *stream) {
        u_char blks = 0;
        u_short cnt;
        int got;
        int rc = 0;
        u_char *mbuf;

        /*
         * Wait for the lenght byte.
         */
        got = fread(&blks, 1, 1, stream);
        if (got != 1) {
            return -1;
        }
        if (blks) {
            if (blks > 32) {
                printf("Error: Metadata too large, %u blocks\n", blks);
                return -1;
            }

            cnt = blks * 16;
            if ((mbuf = malloc(cnt + 1)) == 0) {
                return -1;
            }

            /*
             * Receive the metadata block.
             */
            for (; ;) {
                if ((got = fread(mbuf + rc, 1, cnt, stream)) <= 0) {
                    return -1;
                }
                if ((cnt -= got) == 0) {
                    break;
                }
                rc += got;
                mbuf[rc] = 0;
            }

            printf("\nMeta='%s'\n", mbuf);
            free(mbuf);
        }
        return 0;
    }

    /*
 * \brief Play MP3 stream.
 *
 * \param stream Socket stream to read MP3 data from.
 */
    void PlayMp3Stream(FILE *stream, u_long metaint) {
        size_t rbytes;
        u_char *mp3buf;
        u_char ief;
        int got = 0;
        u_long last;
        u_long mp3left = metaint;

        /*
         * Initialize the MP3 buffer. The NutSegBuf routines provide a global
         * system buffer, which works with banked and non-banked systems.
         */
        if (NutSegBufInit(8192) == 0) {
            puts("Error: MP3 buffer init failed");
            return;
        }

        /*
         * Initialize the MP3 decoder hardware.
         */
        if (VsPlayerInit() || VsPlayerReset(0)) {
            puts("Error: MP3 hardware init failed");
            return;
        }
        VsSetVolume(0, 0);

        /*
         * Reset the MP3 buffer.
         */
        ief = VsPlayerInterrupts(0);
        NutSegBufReset();
        VsPlayerInterrupts(ief);
        last = NutGetSeconds();

        for (; ;) {

            if(STOP_THREAD){
                STOP_THREAD = 0;
                return;
            }

            /*
             * Query number of byte available in MP3 buffer.
             */
            ief = VsPlayerInterrupts(0);
            mp3buf = NutSegBufWriteRequest(&rbytes);
            VsPlayerInterrupts(ief);

            /*
             * If the player is not running, kick it.
             */
            if (VsGetStatus() != VS_STATUS_RUNNING) {
                if (rbytes < 1024 || NutGetSeconds() - last > 4UL) {
                    last = NutGetSeconds();
                    puts("Kick player");
                    VsPlayerKick();
                }
            }

            /*
             * Do not read pass metadata.
             */
            if (metaint && rbytes > mp3left) {
                rbytes = mp3left;
            }

            /*
             * Read data directly into the MP3 buffer.
             */
            while (rbytes) {
                if ((got = fread(mp3buf, 1, rbytes, stream)) > 0) {
                    ief = VsPlayerInterrupts(0);
                    mp3buf = NutSegBufWriteCommit(got);
                    VsPlayerInterrupts(ief);

                    if (metaint) {
                        mp3left -= got;
                        if (mp3left == 0) {
                            ProcessMetaData(stream);
                            mp3left = metaint;
                        }
                    }

                    if (got < rbytes && got < 512) {
                        printf("%lu buffered\n", NutSegBufUsed());
                        NutSleep(250);
                    }
                    else {
                        NutThreadYield();
                    }
                } else {
                    break;
                }
                rbytes -= got;
            }

            if (got <= 0) {
                break;
            }

            NutSleep(100);
        }
    }

    int send_message(u_long ip, u_short port, u_long *metaint){
        int rc;
        FILE *stream;
        u_char *line;
        u_char *cp;
        TCPSOCKET *sock;

        u_long baud = DBG_BAUDRATE;
//        u_long radio_ip = inet_addr(RADIO_IPADDR);
        u_short tcpbufsiz = TCPIP_BUFSIZ;
        u_long rx_to = TCPIP_READTIMEOUT;
        u_short mss = TCPIP_MSS;
        puts("create a TCP socket");

        /*
         * Create a TCP socket.
         */
        if ((sock = NutTcpCreateSocket()) == 0) {
            puts("Error: Can't create socket");
            for (; ;);
        }

        puts("set socket options");

        /*
         * Set socket options. Failures are ignored.
         */
        if (NutTcpSetSockOpt(sock, TCP_MAXSEG, &mss, sizeof(mss)))
            printf("Sockopt MSS failed\n");
        if (NutTcpSetSockOpt(sock, SO_RCVTIMEO, &rx_to, sizeof(rx_to)))
            printf("Sockopt TO failed\n");
        if (NutTcpSetSockOpt(sock, SO_RCVBUF, &tcpbufsiz, sizeof(tcpbufsiz)))
            printf("Sockopt rxbuf failed\n");

        /*
         * Connect the TCP server.
         */

        printf("Connecting %s:%u...","83.128.250.123" , 8080);
        if ((rc = NutTcpConnect(sock, inet_addr("83.128.250.123"), 8080))) {
            printf("Error: Connect failed with %d\n", NutTcpError(sock));
            return 0;
        }
        puts("OK");

        if ((stream = _fdopen((int) sock, "r+b")) == 0) {
            puts("Error: Can't create stream");
            return 0;
        }

        /*
         * Send the HTTP request.
         */
        printf("GET %s HTTP/1.0\n\n", "/api/telegram");
        fprintf(stream, "GET %s HTTP/1.0\r\n", "/api/telegram");
        fprintf(stream, "Host: %s\r\n", "83.128.250.123");
        fprintf(stream, "User-Agent: Ethernut\r\n");
        fprintf(stream, "Accept: */*\r\n");
        fprintf(stream, "Connection: close\r\n");
        fputs("\r\n", stream);
        fflush(stream);

        /*
         * Receive the HTTP header.
         */
        line = malloc(MAX_HEADERLINE);
        while (fgets(line, MAX_HEADERLINE, stream)) {

            /*
             * Chop off the carriage return at the end of the line. If none
             * was found, then this line was probably too large for our buffer.
             */
            cp = strchr(line, '\r');
            if (cp == 0) {
                puts("Warning: Input buffer overflow");
                continue;
            }
            *cp = 0;

            /*
             * The header is terminated by an empty line.
             */
            if (*line == 0) {
                break;
            }
            printf("%s\n", line);
        }
        putchar('\n');

        free(line);
        return 0;
    }

/*
 * Main application entry.
 */
    int play_stream(RADIO_STREAM rStream) {
        TCPSOCKET *sock;
        FILE *stream;
        u_long baud = DBG_BAUDRATE;
        u_long radio_ip = inet_addr(rStream.radio_ip);
        u_short tcpbufsiz = TCPIP_BUFSIZ;
        u_long rx_to = TCPIP_READTIMEOUT;
        u_short mss = TCPIP_MSS;
        u_long metaint;

        /*
         * Register UART device and assign stdout to it.
         */
        NutRegisterDevice(&DBG_DEVICE, 0, 0);
        freopen(DBG_DEVNAME, "w", stdout);
        _ioctl(_fileno(stdout), UART_SETSPEED, &baud);

        /*
         * Display system information.
         */
        printf("\n\nMedianut Tuotrial Part 3 - Nut/OS %s - " CC_STRING "\n", NutVersionString());
        printf("%u bytes free\n\n", NutHeapAvailable());

        /*
         * Register LAN device.
         */
        if (NutRegisterDevice(&DEV_ETHER, 0x8300, 5)) {
            puts("Error: No LAN device");
            for (; ;);
        }

        puts("configure LAN");
        /*
         * Configure LAN.
         */
        if (ConfigureLan("eth0")) {
            for (; ;);
        }

        puts("create a TCP socket");

        /*
         * Create a TCP socket.
         */
        if ((sock = NutTcpCreateSocket()) == 0) {
            puts("Error: Can't create socket");
            for (; ;);
        }

        puts("set socket options");

        /*
         * Set socket options. Failures are ignored.
         */
        if (NutTcpSetSockOpt(sock, TCP_MAXSEG, &mss, sizeof(mss)))
            printf("Sockopt MSS failed\n");
        if (NutTcpSetSockOpt(sock, SO_RCVTIMEO, &rx_to, sizeof(rx_to)))
            printf("Sockopt TO failed\n");
        if (NutTcpSetSockOpt(sock, SO_RCVBUF, &tcpbufsiz, sizeof(tcpbufsiz)))
            printf("Sockopt rxbuf failed\n");


        puts("connect the radio station");
        /*
         * Connect the radio station.
         */
        radio_ip = inet_addr(rStream.radio_ip);
        stream = ConnectStation(sock, rStream.radio_ip, rStream.radio_port, &metaint, rStream);
		send_message(rStream.radio_ip, rStream.radio_port, &metaint);
        /*
         * Play the stream.
         */
        if (stream) {
            puts("playing stream");
            PlayMp3Stream(stream, metaint);
            fclose(stream);
        }
        NutTcpCloseSocket(sock);


        NutThreadKill();
        NutThreadDestroy();
        return;
        puts("Reset me!");
        for (; ;);
}

void initAudioStreams(){
    yorick = malloc(sizeof(RADIO_STREAM));
    yorick->name="yorick";
    yorick->radio_port=9999;
    yorick->radio_ip="83.128.250.123";
    yorick->radio_url="/mpd.mp3";

    radio_3fm = malloc(sizeof(RADIO_STREAM));
    radio_3fm->name="funx dance";
    radio_3fm->radio_port=80;
    radio_3fm->radio_ip="145.58.52.152";
    radio_3fm->radio_url="/funx-dancefb-bb-mp3";

    funx_reggae = malloc(sizeof(RADIO_STREAM));
    funx_reggae->name="funx reggae";
    funx_reggae->radio_port=80;
    funx_reggae->radio_ip="145.58.52.152";
    funx_reggae->radio_url="/funx-latinfb-bb-mp3";

}

THREAD(PlayStream, args){
    RADIO_STREAM *stream = (RADIO_STREAM*) args;
    play_stream(*stream);
}
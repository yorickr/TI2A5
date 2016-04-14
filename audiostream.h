//
// Created by Gilian Joosen on 10/03/16.
//
#ifndef MULTI_OS_BUILD_AUDIOSTREAM_H
#define MULTI_OS_BUILD_AUDIOSTREAM_H

int STOP_THREAD;

typedef struct radio_stream {
    char *name; //name
    char *radio_ip; //ip
    char *radio_url; //url
    int radio_port;
} RADIO_STREAM;

RADIO_STREAM *yorick;
RADIO_STREAM *radio_3fm;
RADIO_STREAM *funx_reggae;

void initAudioStreams(void);

int play_stream(RADIO_STREAM rStream);
int streamdone;

THREAD(PlayStream, args);

#endif //MULTI_OS_BUILD_AUDIOSTREAM_H

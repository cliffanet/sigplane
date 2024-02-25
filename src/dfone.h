/*
    DF play one file
*/

#ifndef _df_one_H
#define _df_one_H

#include "dfproto.h"
#include <Stream.h>

// основано на модуле DFRobotDFPlayerMini

class DFOne {
    DFStream<> _rcv;
    int8_t _pin;
    Stream *_s;

    enum {
        INIT,
        OFF,
        START,
        SENDED,
        PLAY
    } _state;

    uint8_t _fold, _file;

    void _send(uint8_t fold, uint8_t file);

    public:
        DFOne(Stream &str, int8_t pinen = -1) :
            _s(&str),
            _pin(pinen),
            _state(INIT),
            _fold(0),
            _file(0)
            {};
        void begin();
        void end();
        void play(uint8_t foldnum, uint8_t filenum);
        void recv();
};

#endif // _df_one_H

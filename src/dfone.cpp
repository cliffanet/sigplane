
#include "dfone.h"
#include <Arduino.h>

void DFOne::_send(uint8_t fold, uint8_t file) {
    uint8_t buf[2] = { fold, file };
    auto d = DFBin<2>(0x0F, buf);
    _s->write(d.buf(), d.sz());
    _state = SENDED;
}

void DFOne::begin() {
    if (_pin >= 0)
        pinMode(_pin, OUTPUT);
    _state = OFF;
}

void DFOne::end() {
    if (_pin >= 0)
        pinMode(_pin, INPUT);
    _state = INIT;
}

void DFOne::play(uint8_t foldnum, uint8_t filenum) {
    if ((_pin >= 0) && (_state == OFF)) {
        digitalWrite(_pin, HIGH);
        _fold = foldnum;
        _file = filenum;
        _state = START;
    }
    else
    if (_state > INIT) {
        _fold = 0;
        _file = 0;
        _send(foldnum, filenum);
    }
}

void DFOne::recv() {
    _rcv.recv(*_s);
    if (_rcv.iserr()) {
        _rcv.clear();
        _s->flush();
        _state = OFF;
    }
    else
    if (_rcv.valid()) {
        switch (_rcv.cmd()) {
            case 0x3f: // online
            case 0x3a: // storage inserted
                if ((_state == START) && (_rcv.asuint16() == 2)) {
                    _send(_fold, _file);
                    _fold = 0;
                    _file = 0;
                }
                break;
            
            case 0x41: // cmd confirm
                if (_state == SENDED)
                    _state = PLAY;
                break;
            
            case 0x3d: // play end
                if (_state == PLAY) {
                    _state = OFF;
                    if (_pin >= 0) {
                        digitalWrite(_pin, LOW);
                        _rcv.clear();
                        _s->flush();
                        return;
                    }
                }
                break;
        }
        _rcv.next();
    }
}

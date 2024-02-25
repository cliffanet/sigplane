/*
    DF pack/unpack
*/

#ifndef _df_data_H
#define _df_data_H

#include <stdint.h>
#include <cstddef>
#include <Stream.h>

#define DFPROTO_HEAD    0x7E
#define DFPROTO_VER     0xFF
#define DFPROTO_END     0xEF

void dfpk(uint8_t *buf, const uint8_t &cmd, const uint8_t *data, const uint8_t &sz, bool ack = false, uint8_t ver = DFPROTO_VER);

template <int S>
class DFBin {
    uint8_t _b[S + 8];

    public:
        DFBin(uint8_t cmd, const uint8_t *data, bool ack = true) {
            dfpk(_b, cmd, data, S, ack);
        }

        size_t sz()     const { return sizeof(_b); }
        size_t len()    const { return S; }
        const uint8_t *buf() const { return _b; }
};

template <typename T>
DFBin<sizeof(T)> DFData(uint8_t cmd, const T &data, bool ack = true) {
    return DFBin<sizeof(T)>(cmd, reinterpret_cast<const uint8_t*>(&data), ack);
}

DFBin<2> DFuint16(uint8_t cmd, uint16_t val, bool ack = true);

inline
DFBin<0> DFCmd(uint8_t cmd, bool ack = true) {
    return DFBin<0>(cmd, NULL, ack);
}

class DFRecv {
    const uint8_t *_b;
    uint8_t _cmd;
    uint8_t  _len;
    typedef enum {
        OK = 0,
        SHORT,
        PROTO
    } state_t;
    state_t _state;
    public:
        DFRecv(const uint8_t *buf, size_t sz);
        void parse(size_t sz);

        bool valid()    const { return _state == OK; }
        operator bool() const { return _state == OK; }
        bool iserr()    const { return _state == PROTO; }
        state_t state() const { return _state; }
        uint8_t cmd()   const { return _cmd; }
        uint8_t sz()    const { return _len+8; }
        uint8_t len()   const { return _len; }
        
        void data(uint8_t *buf, size_t sz);
        template <typename T>
        T data() {
            // Можно было бы вернуть просто: const T&
            // сделав простой возврат: *reinterpret_cast<uint8_t*>(_b+4)
            // Но тут есть большой риск, что придут данные, короче, чем T,
            // и тогда будем иметь неприятные вылеты. Поэтому лучше лишний
            // раз скопировать данные и обнулить хвост при необходимости.
            T d;
            data(reinterpret_cast<uint8_t*>(&d), sizeof(T));
            return d;
        }

        uint16_t asuint16();
};

template <int S = 128>
class DFStream : public DFRecv {
    uint8_t _buf[S];
    size_t _cur;
    
    public:
        DFStream() :
            DFRecv(_buf, S),
            _cur(0)
        {

        }

        void recv(Stream &s) {
            while ((s.available() > 0) && (_cur < S)) {
                _buf[_cur] = s.read();
                if ((_cur == 0) && (*_buf != DFPROTO_HEAD))
                    continue;
                _cur ++;
            }
            parse(_cur);
        }

        bool next() {
            if (!valid() || (_cur < sz()))
                return false;
            size_t c1 = 0;
            auto c2 = sz();
            while (c2 < _cur) {
                _buf[c1] = _buf[c2];
                c1++;
                c2++;
            }
            _cur -= sz();
            parse(_cur);

            return true;
        }

        void clear() {
            _cur = 0;
        }
};

#endif // _df_data_H

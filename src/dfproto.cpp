
#include "dfproto.h"

#include <cstring>

static void ck(uint8_t &a, uint8_t &b, const uint8_t *data, uint8_t sz) {
    uint16_t s = 0;
    while (sz > 0) {
        s += *data;
        data ++;
        sz--;
    }
    s = -s;
    a = (s & 0xff00) >> 8;
    b = s & 0xff;
}

void dfpk(uint8_t *buf, const uint8_t &cmd, const uint8_t *data, const uint8_t &sz, bool ack, uint8_t ver) {
    buf[0] = DFPROTO_HEAD;  // header
    buf[1] = ver;           // version
    buf[2] = sz+4;          // length
    buf[3] = cmd;           // command
    buf[4] = ack ? 1 : 0;   // ack
    auto b = buf+5;         // param
    memcpy(b, data, sz);
    b += sz;                // checksum
    ck(b[0], b[1], buf+1, sz+4);
    b[2] = DFPROTO_END;     // end
}

DFBin<2> DFuint16(uint8_t cmd, uint16_t val, bool ack) {
    uint8_t d[2] = {
        (val & 0xff00) >> 8,
        val & 0xff
    };
    return DFBin<2>(cmd, d, ack);
}

DFRecv::DFRecv(const uint8_t *buf, size_t sz) :
    _b(buf)
{ parse(sz); }

void DFRecv::parse(size_t sz) {
    if (sz < 8) {
        _state = SHORT;
        _cmd = 0;
        _len = 0;
        return;
    }
    if ((_b[0] != DFPROTO_HEAD) || (_b[1] != DFPROTO_VER) || (_b[2] < 4)) { // header + version + length
        _state = PROTO;
        _cmd = 0;
        _len = 0;
        return;
    }

    _len = _b[2]-4;
    _cmd = _b[3];
    if (sz < (_len+8)) {
        _state = SHORT;
        return;
    }

    // checksum + end
    uint8_t cka, ckb;
    ck(cka, ckb, _b+1, _len+4);
    _state = (_b[_len+5] == cka) && (_b[_len+6] == ckb) && (_b[_len+7] == DFPROTO_END) ?
        OK :
        PROTO;
}

void DFRecv::data(uint8_t *buf, size_t sz) {
    if (_state != OK) return;
    if (sz > _len) {
        memcpy(buf, _b+5, _len);
        memset(buf+_len, 0, sz-_len);
    }
    else
        memcpy(buf, _b+5, sz);
}

uint16_t DFRecv::asuint16() {
    uint8_t d[2];
    data(d, sizeof(d));
    return (static_cast<uint16_t>(d[0]) << 8) | d[1];
}


#ifdef HW_VER0
#define BTN_RED       35
#define BTN_BLUE      34
#define BTN_YELLOW    33
#define BTN_GREEN     32
#else
#define BTN_RED       34
#define BTN_BLUE      35
#define BTN_YELLOW    32
#define BTN_GREEN     33
#endif

#define LED_RED       23
#define LED_BLUE      19
#define LED_GREEN     18

#define AUDEN         25

#include <stdint.h>
#include <string.h>

#include "src/dfone.h"


/* log */

#define LOG_FMT(s)        PSTR(" [%u] %s(): " s), __LINE__, __FUNCTION__
#define log(txt, ...)     log_P(LOG_FMT(txt), ##__VA_ARGS__)
static void vlog(const char *s, va_list ap) {
    int len = vsnprintf(NULL, 0, s, ap), sbeg = 0;
    char str[len+60]; // +48=dt +12=debug mill/tick
    
    uint64_t ms = millis();
    uint32_t d = ms / (3600*24*1000);
    uint8_t h = (ms - d*3600*24*1000) / (3600*1000);
    uint8_t m = (ms - d*3600*24*1000 - h*3600*1000) / (60*1000);
    uint8_t ss = (ms - d*3600*24*1000 - h*3600*1000 - m*60*1000) / 1000;
    ms -= d*3600*24*1000 + h*3600*1000 + m*60*1000 + ss*1000;
    sbeg = snprintf_P(str, 32, PSTR("%3ud %2u:%02u:%02u.%03llu"), d, h, m, ss, ms);
    
    vsnprintf(str+sbeg, len+1, s, ap);
    
    Serial.println(str);
}
static void log_P(const char *s, ...) {
    va_list ap;
    char str[strlen_P(s)+1];

    strcpy_P(str, s);
    
    va_start (ap, s);
    vlog(str, ap);
    va_end (ap);
}

/* mp3 */
DFOne mp3(Serial2, AUDEN);

/* btn */

typedef struct {
    uint8_t     pin;
    void        (*hnd)(uint32_t c);
    uint8_t     mp3;
    uint8_t     pushed;
} btn_t;
static volatile btn_t btnall[] = {
    { BTN_RED,    show_r, 1 },
    { BTN_BLUE,   show_b, 2 },
    { BTN_YELLOW, show_y, 3 },
    { BTN_GREEN,  show_g, 4 },
};

#define pinbit(pin)     (static_cast<uint64_t>(1) << (pin))

static void pwrDeepSleep(uint64_t timer = 0);

/* led */

static void clear() {
    digitalWrite(LED_RED,   LOW);
    digitalWrite(LED_BLUE,  LOW);
    digitalWrite(LED_GREEN, LOW);
}

static void show_r(uint32_t c) {
    digitalWrite(LED_RED,   HIGH);
}
static void show_b(uint32_t c) {
    digitalWrite(LED_BLUE,  (c%10) < 5 ? HIGH : LOW);
    if (c >= 35)
        pwrDeepSleep();
}
static void show_y(uint32_t c) {
    digitalWrite(LED_BLUE,  HIGH);
}
static void show_g(uint32_t c) {
    digitalWrite(LED_GREEN, HIGH);
}

/* btn interrupt */

void IRAM_ATTR btnChkState(volatile btn_t &b, bool fastoff = false) {
    uint8_t val = digitalRead(b.pin);
    if (val == HIGH)
        b.pushed = 5;
    else
    if (b.pushed > 0) {
        if (fastoff)
            b.pushed = 0;
        else
            b.pushed --;
    }
}
void IRAM_ATTR btnChkState0() { btnChkState(btnall[0]); }
void IRAM_ATTR btnChkState1() { btnChkState(btnall[1]); }
void IRAM_ATTR btnChkState2() { btnChkState(btnall[2]); }
void IRAM_ATTR btnChkState3() { btnChkState(btnall[3]); }

bool btnpushed() {
    for (const auto &b: btnall)
        if (b.pushed)
            return true;

    return false;
}
void btnchkall(bool fastoff = false) {
    for (auto &b: btnall)
        btnChkState(b, fastoff);
}

/* sleep */

// Уход в сон с таймером - sleep
static void pwrDeepSleep(uint64_t timer) {
    // ожидаем, пока все кнопки будут отпущены
    while (btnpushed()) {
        delay(30);
        btnchkall(true);
    }

    uint64_t m = 0;
    for (const auto &b: btnall) m |= pinbit(b.pin);
    auto r = esp_sleep_enable_ext1_wakeup(m, ESP_EXT1_WAKEUP_ANY_HIGH);
    //esp_sleep_enable_ext0_wakeup(GPIO_NUM_35,1);
    switch (r) {
        case ESP_OK:              log("ext1 ok"); break;
        case ESP_ERR_INVALID_ARG: log("ext1 invalid arg"); break;
        default: log("ext1 %d", r);
    }
    
    if (timer > 0)
        esp_sleep_enable_timer_wakeup(timer);

    //Go to sleep now
    log("Going to deep sleep now");
    esp_deep_sleep_start();
    log("This will never be printed");
}

/* init */

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);

    for (auto &b: btnall)
        pinMode(b.pin, INPUT);
    
    auto wakeup_reason = esp_sleep_get_wakeup_cause();
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0 :        log("Wakeup caused by external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1 :
            log("Wakeup caused by external signal using RTC_CNTL");
            {
                auto p = esp_sleep_get_ext1_wakeup_status();
                for (const auto &b: btnall)
                    if ((p & pinbit(b.pin)) > 0)
                        log("Wakeup pin: %d", b.pin);
            }
            break;
        case ESP_SLEEP_WAKEUP_TIMER :       log("Wakeup caused by timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD :    log("Wakeup caused by touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP :         log("Wakeup caused by ULP program"); break;
        default : log("Wakeup was not caused by deep sleep: %d", wakeup_reason); break;
    }

    btnchkall();
    if (!btnpushed())
        pwrDeepSleep();

    attachInterrupt(
        digitalPinToInterrupt(btnall[0].pin),
        btnChkState0,
        CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(btnall[1].pin),
        btnChkState1,
        CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(btnall[2].pin),
        btnChkState2,
        CHANGE
    );
    attachInterrupt(
        digitalPinToInterrupt(btnall[3].pin),
        btnChkState3,
        CHANGE
    );

    pinMode(LED_RED,        OUTPUT);
    pinMode(LED_BLUE,       OUTPUT);
    pinMode(LED_GREEN,      OUTPUT);

    Serial2.begin(9600);
    mp3.begin();

    log("start");
}

void loop() {
    // put your main code here, to run repeatedly:

    static const volatile btn_t *r = NULL;
    static uint32_t c = 0;
    static uint64_t pushed = 0;

    for (const auto &b: btnall) {
        uint64_t bit = pinbit(b.pin);
        bool psh = (pushed & bit) > 0;
        if (b.pushed) {
            if (psh)
                continue;
            
            log("[%d] push", b.pin);
            pushed |= bit;
            c = 0;
            clear();

            if (r == &b) {
                log("[%d] off", b.pin);
                r = NULL;
                pwrDeepSleep();
            }
            else {
                log("[%d] on", b.pin);
                r = &b;
                if (b.mp3 > 0)
                    mp3.play(1, b.mp3);
            }
        }
        else
        if (psh) {
            log("[%d] release", b.pin);
            pushed &= ~bit;
        }
    }

    c++;
    if (c > 1800)
        pwrDeepSleep();

    if (r != NULL)
        r->hnd( c );
    
    mp3.recv();
    
    delay(100);

    btnchkall();
}

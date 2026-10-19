#include <Arduino.h>

// ---------- Налаштування пінів ----------
#define PIN_GREEN 5
#define PIN_YELLOW 6
#define PIN_RED 7
#define PIN_BUTTON 4

// ---------- Тривалості (мілісекунди) ----------
#define T_GREEN_MS 10000UL          // зелений: 10 с
#define T_GREEN_BLINK_MS 3000UL     // зелений миготливий: 3 с
#define T_YELLOW_MS 3000UL          // жовтий: 3 с
#define T_RED_MS 10000UL            // червоний: 10 с
#define T_RED_YELLOW_MS 2000UL      // червоний + жовтий: 2 с
#define BLINK_HALF_PERIOD_MS 500UL  // півперіод миготіння: 0,5 с
#define DEBOUNCE_MS 200UL           // антидребезг кнопки

// Тактування таймера: 80 МГц (APB) / 80 = 1 МГц => 1 тік = 1 мкс
#define TIMER_DIVIDER 80
#define MS_TO_TICKS(ms) ((uint64_t)(ms) * 1000ULL)

// ---------- Стани автомата ----------
enum TrafficState {
    STATE_GREEN = 0,    // зелений
    STATE_GREEN_BLINK,  // зелений миготливий
    STATE_YELLOW,       // жовтий
    STATE_RED,          // червоний
    STATE_RED_YELLOW,   // червоний + жовтий
    STATE_YELLOW_BLINK  // особливий режим: жовтий миготливий
};

// ---------- Події ----------
enum TrafficEvent {
    EVT_STATE_TIMEOUT = 1,  // спрацював таймер тривалості стану
    EVT_BLINK_TICK,         // тік таймера миготіння
    EVT_BUTTON              // натиснута кнопка
};

// ---------- Налаштування станів: стан + тривалість ----------
struct StateSetup {
    TrafficState state;
    uint32_t delay;  // мс
};

// ---------- послідовність станів для нормального стейта ----------
const StateSetup statesSetup[] = {
    {STATE_GREEN, T_GREEN_MS},           {STATE_GREEN_BLINK, T_GREEN_BLINK_MS},
    {STATE_YELLOW, T_YELLOW_MS},         {STATE_RED, T_RED_MS},
    {STATE_RED_YELLOW, T_RED_YELLOW_MS}, {STATE_YELLOW_BLINK, BLINK_HALF_PERIOD_MS},
};

// ----------  спеціальний стейт ----------
const StateSetup specialSetup = {STATE_YELLOW_BLINK, BLINK_HALF_PERIOD_MS};

// ----------  поточний стейт ----------
StateSetup currentSetup;

hw_timer_t* timer_states = NULL;
hw_timer_t* timer_ticks = NULL;

volatile int counter = 0;
volatile bool stateChanged = false;

volatile bool isSpecialMode = false;

volatile bool isModeChanged = false;

volatile int tickCounter = 0;

unsigned long button_prev_time = 0;

// logger
unsigned long previous_millis = 0;
const unsigned long INTERVAL_MS = 1000UL;  // інтервал логування: 1 с

void IRAM_ATTR onTimer() {
    counter++;
    stateChanged = true;
}

void IRAM_ATTR onTick() { tickCounter++; }

void IRAM_ATTR onChangeModeWithDebounce() {
    unsigned long now = millis();

    if (now - button_prev_time < DEBOUNCE_MS) return;
    isModeChanged = true;
    button_prev_time = now;
}

void setupTimerState() {
    timer_states = timerBegin(0, TIMER_DIVIDER, true);
    timerAttachInterrupt(timer_states, &onTimer, true);
    timerAlarmWrite(timer_states, MS_TO_TICKS(currentSetup.delay), true);
    timerAlarmEnable(timer_states);
}

void setupTimerTick() {
    timer_ticks = timerBegin(1, TIMER_DIVIDER, true);
    timerAttachInterrupt(timer_ticks, &onTick, true);
    timerAlarmWrite(timer_ticks, MS_TO_TICKS(BLINK_HALF_PERIOD_MS), true);
    timerAlarmEnable(timer_ticks);
}

void setup() {
    pinMode(PIN_GREEN, OUTPUT);
    pinMode(PIN_YELLOW, OUTPUT);
    pinMode(PIN_RED, OUTPUT);

    pinMode(PIN_BUTTON, INPUT_PULLUP);

    currentSetup = statesSetup[0];

    setupTimerState();
    setupTimerTick();

    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), &onChangeModeWithDebounce, FALLING);

    Serial.begin(115200);
}

void loop() {
    if (isModeChanged) {
        isSpecialMode = !isSpecialMode;
        isModeChanged = false;
        counter = 0;
        if (isSpecialMode) {
            currentSetup = specialSetup;
        } else {
            currentSetup = statesSetup[counter];
        }
    }

    if (isSpecialMode) {
        digitalWrite(PIN_GREEN, LOW);
        digitalWrite(PIN_YELLOW, tickCounter % 2 == 0 ? HIGH : LOW);
        digitalWrite(PIN_RED, LOW);
    } else {
        if (stateChanged) {
            const int maxStates = sizeof(statesSetup) / sizeof(statesSetup[0]) - 1;
            if (counter >= maxStates) {
                counter = 0;
            }
            currentSetup = statesSetup[counter];
            stateChanged = false;
        }

        switch (currentSetup.state) {
            case STATE_GREEN:
                digitalWrite(PIN_GREEN, HIGH);
                digitalWrite(PIN_YELLOW, LOW);
                digitalWrite(PIN_RED, LOW);
                break;
            case STATE_GREEN_BLINK:
                digitalWrite(PIN_GREEN, tickCounter % 2 == 0 ? HIGH : LOW);
                digitalWrite(PIN_YELLOW, LOW);
                digitalWrite(PIN_RED, LOW);
                break;
            case STATE_YELLOW:
                digitalWrite(PIN_GREEN, LOW);
                digitalWrite(PIN_YELLOW, HIGH);
                digitalWrite(PIN_RED, LOW);
                break;
            case STATE_YELLOW_BLINK:
                digitalWrite(PIN_GREEN, LOW);
                digitalWrite(PIN_YELLOW, tickCounter % 2 == 0 ? HIGH : LOW);
                digitalWrite(PIN_RED, LOW);
                break;
            case STATE_RED:
                digitalWrite(PIN_GREEN, LOW);
                digitalWrite(PIN_YELLOW, LOW);
                digitalWrite(PIN_RED, HIGH);
                break;
            case STATE_RED_YELLOW:
                digitalWrite(PIN_GREEN, LOW);
                digitalWrite(PIN_YELLOW, HIGH);
                digitalWrite(PIN_RED, HIGH);
                break;
        }
    }

    const unsigned long current_millis = millis();
    if (current_millis - previous_millis >= INTERVAL_MS) {
        previous_millis = current_millis;

        Serial.println("[current mode:" + String(isSpecialMode ? "special" : "normal") + "]");
        Serial.println("[current counter:" + String(counter) + "]");
    }
}

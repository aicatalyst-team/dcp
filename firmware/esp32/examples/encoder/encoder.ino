// Recipe: KY-040 rotary encoder + push button — event-only device.
//
// This is the canonical "the device has NO intents" pattern:
// the device only PUSHES events to the LLM, never receives commands.
// The bindings[] array is empty; only send_event() calls happen.
//
// Useful for: volume knobs, menu wheels, RPM counters, anything where
// the LLM should react to user input but never drive the input itself.
//
// Pair with:
//   dcp serve examples/encoder_manifest.yaml --serial COM3
//
// Hardware: KY-040 module
//   CLK -> GPIO 18
//   DT  -> GPIO 19
//   SW  -> GPIO 21 (button, active LOW)
//   VCC -> 3.3V, GND -> GND
//
// All three pins use INPUT_PULLUP. No external resistors needed for KY-040
// (it has its own pullups on CLK/DT but pull internally for redundancy).

#include "DCP.h"

constexpr int PIN_CLK    = 18;
constexpr int PIN_DT     = 19;
constexpr int PIN_BUTTON = 21;

constexpr uint32_t DEBOUNCE_MS         = 5;
constexpr uint32_t LONG_PRESS_MS       = 1000;
constexpr uint32_t BUTTON_DEBOUNCE_MS  = 20;

static int32_t  g_position       = 0;
static int      g_last_clk       = HIGH;
static bool     g_button_down    = false;
static uint32_t g_button_press_t = 0;
static bool     g_long_press_emitted = false;
static uint32_t g_last_clk_change_ms = 0;
static uint32_t g_last_btn_change_ms = 0;

// Event payload buffer (sized generously for 1-2 small fields).
static uint8_t event_buf[48];

static dcp::IntentBinding bindings[] = {
    // INTENTIONALLY EMPTY — this device only pushes events.
};

static dcp::DCP* dcp_instance = nullptr;

static void emit_encoder_turned(int delta) {
    g_position += delta;
    dcp::CborMap m(event_buf, sizeof(event_buf));
    m.begin();
    m.add_int("delta",    (int64_t)delta);
    m.add_int("position", (int64_t)g_position);
    if (dcp_instance) dcp_instance->send_event("encoder_turned", m);
}

static void emit_button_pressed() {
    dcp::CborMap m(event_buf, sizeof(event_buf));
    m.begin();
    // No payload — empty map signals "just the event happened."
    if (dcp_instance) dcp_instance->send_event("button_pressed", m);
}

static void emit_button_long_press(uint32_t held_ms) {
    dcp::CborMap m(event_buf, sizeof(event_buf));
    m.begin();
    m.add_int("held_ms", (int64_t)held_ms);
    if (dcp_instance) dcp_instance->send_event("button_long_press", m);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_CLK,    INPUT_PULLUP);
    pinMode(PIN_DT,     INPUT_PULLUP);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    g_last_clk = digitalRead(PIN_CLK);
    static dcp::DCP instance(Serial, bindings, sizeof(bindings) / sizeof(bindings[0]));
    dcp_instance = &instance;
}

void loop() {
    if (dcp_instance) dcp_instance->poll();

    uint32_t now = millis();

    // ---- Encoder polling (rising-edge of CLK, direction from DT) ----------
    int clk = digitalRead(PIN_CLK);
    if (clk != g_last_clk && (now - g_last_clk_change_ms) >= DEBOUNCE_MS) {
        g_last_clk_change_ms = now;
        if (clk == LOW) {
            // Falling edge: read DT to determine rotation direction.
            int dt = digitalRead(PIN_DT);
            emit_encoder_turned(dt == HIGH ? +1 : -1);
        }
        g_last_clk = clk;
    }

    // ---- Button polling (active-LOW with debounce + long-press) -----------
    bool btn_now = (digitalRead(PIN_BUTTON) == LOW);
    if (btn_now != g_button_down && (now - g_last_btn_change_ms) >= BUTTON_DEBOUNCE_MS) {
        g_last_btn_change_ms = now;
        g_button_down = btn_now;
        if (btn_now) {
            // Press down.
            g_button_press_t      = now;
            g_long_press_emitted  = false;
            emit_button_pressed();
        }
        // (We don't emit a separate "released" event in this recipe.)
    }
    if (g_button_down && !g_long_press_emitted &&
        (now - g_button_press_t) >= LONG_PRESS_MS) {
        emit_button_long_press(now - g_button_press_t);
        g_long_press_emitted = true;
    }
}

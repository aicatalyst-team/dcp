// Recipe: single-channel 5V relay over DCP.
//
// Wiring: relay control pin (IN) -> RELAY_PIN below (default GPIO 5).
// Most modules are ACTIVE-LOW (LOW = relay ENERGISED). Set RELAY_ACTIVE_LOW
// to false if your module is active-high.
//
// Pair with:
//   dcp serve examples/relay_manifest.yaml --serial COM3
//
// Compiles on ESP32 family (Xtensa + RISC-V) and ESP8266 — no PWM needed,
// pure digitalWrite, zero dependencies.

#include "DCP.h"

constexpr int  RELAY_PIN        = 5;       // adjust to your board
constexpr bool RELAY_ACTIVE_LOW = true;    // most blue PCB modules are active-low

static bool g_state = false;

static inline void write_relay(bool on) {
    g_state = on;
    digitalWrite(RELAY_PIN, (on ^ RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

static dcp::Status handle_set_relay(uint8_t kind,
                                    dcp::CborReader& params,
                                    dcp::CborMap& reply,
                                    void* /*user*/) {
    bool state = false;
    while (params.remaining() > 0) {
        const char* key = nullptr;
        size_t key_len = 0;
        if (!params.next_key(&key, &key_len)) return dcp::STATUS_DENIED;
        if (key_len == 5 && memcmp(key, "state", 5) == 0) {
            if (!params.read_bool(&state)) return dcp::STATUS_RANGE;
        } else {
            params.skip();
        }
    }

    if (kind == dcp::KIND_DRY_RUN) {
        reply.add_bool("would_set", state);
        return dcp::STATUS_OK;
    }

    write_relay(state);
    return dcp::STATUS_OK;
}

static dcp::Status handle_read_relay(uint8_t /*kind*/,
                                     dcp::CborReader& /*params*/,
                                     dcp::CborMap& reply,
                                     void* /*user*/) {
    reply.add_bool("value", g_state);
    return dcp::STATUS_OK;
}

static dcp::Status handle_pulse(uint8_t kind,
                                dcp::CborReader& params,
                                dcp::CborMap& reply,
                                void* /*user*/) {
    double duration_ms = 200.0;     // duration is encoded as CBOR float on the wire
    while (params.remaining() > 0) {
        const char* key = nullptr;
        size_t key_len = 0;
        if (!params.next_key(&key, &key_len)) return dcp::STATUS_DENIED;
        if (key_len == 8 && memcmp(key, "duration", 8) == 0) {
            if (!params.read_float(&duration_ms)) return dcp::STATUS_RANGE;
        } else {
            params.skip();
        }
    }
    if (duration_ms < 50.0 || duration_ms > 5000.0) return dcp::STATUS_RANGE;

    uint32_t ms = (uint32_t)duration_ms;
    if (kind == dcp::KIND_DRY_RUN) {
        reply.add_int("would_pulse_ms", (int64_t)ms);
        return dcp::STATUS_OK;
    }

    bool saved = g_state;
    write_relay(true);
    delay(ms);
    write_relay(saved);    // restore prior state, not just OFF
    return dcp::STATUS_OK;
}

static dcp::IntentBinding bindings[] = {
    { DCP_ID("set_relay"),  handle_set_relay,  nullptr },
    { DCP_ID("read_relay"), handle_read_relay, nullptr },
    { DCP_ID("pulse"),      handle_pulse,      nullptr },
};

static dcp::DCP* dcp_instance = nullptr;

void setup() {
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
    write_relay(false);    // start safely OFF
    static dcp::DCP instance(Serial, bindings, sizeof(bindings) / sizeof(bindings[0]));
    dcp_instance = &instance;
}

void loop() {
    if (dcp_instance) dcp_instance->poll();
}

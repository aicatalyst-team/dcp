// Recipe: DHT22 temperature + humidity sensor with threshold events.
//
// Demonstrates the DCP read-intent + unsolicited-event pattern. The
// actual DHT22 driver call is stubbed (read_dht_*() returns mock data) —
// drop in your favourite library (e.g. Adafruit DHT) at those two points.
//
// Pair with:
//   dcp serve examples/sensor_dht22_manifest.yaml --serial COM3
//
// The Bridge fans threshold_exceeded events out to any LLM session that
// holds env.read capability, so the LLM can react to a hot room without
// polling.
//
// Hardware (when you wire a real DHT22):
//   - DHT22 data pin -> DHT_PIN (default GPIO 4)
//   - 4.7 KOhm pullup from data pin to VCC
//   - VCC = 3.3V or 5V, GND to GND
//
// Wiring up a real sensor: see the README in this directory.

#include "DCP.h"

constexpr int DHT_PIN = 4;            // adjust to your board

// State the device tracks
static float g_threshold_c = 30.0f;   // alert threshold, mutable via intent
static float g_last_temp_c = 0.0f;
static bool  g_above_last  = false;   // for edge-triggered event emission
static uint32_t g_next_sample_ms = 0;
constexpr uint32_t SAMPLE_INTERVAL_MS = 2000;

static dcp::DCP* dcp_instance = nullptr;

// ---- Sensor driver stubs ---------------------------------------------------
// Replace these with your DHT22 library calls. The Adafruit DHT library:
//   #include <DHT.h>
//   DHT dht(DHT_PIN, DHT22);
//   dht.begin();                 // in setup()
//   float t = dht.readTemperature();
//   float h = dht.readHumidity();

static float read_dht_temperature_c() {
    // STUB — returns a slowly drifting fake value so the example runs
    // headlessly during compile-test. Replace with real driver call.
    return 22.0f + (float)((millis() / 200) % 200) * 0.05f;   // 22..32 C drift
}

static float read_dht_humidity_pct() {
    // STUB — returns a fake reasonable humidity.
    return 45.0f + (float)((millis() / 300) % 100) * 0.1f;    // 45..55 %
}
// ---------------------------------------------------------------------------

static dcp::Status handle_read_temperature(uint8_t /*kind*/,
                                           dcp::CborReader& /*params*/,
                                           dcp::CborMap& reply,
                                           void* /*user*/) {
    float t = read_dht_temperature_c();
    reply.add_float("value", (double)t);
    return dcp::STATUS_OK;
}

static dcp::Status handle_read_humidity(uint8_t /*kind*/,
                                        dcp::CborReader& /*params*/,
                                        dcp::CborMap& reply,
                                        void* /*user*/) {
    float h = read_dht_humidity_pct();
    reply.add_float("value", (double)h);
    return dcp::STATUS_OK;
}

static dcp::Status handle_set_alert_threshold(uint8_t kind,
                                              dcp::CborReader& params,
                                              dcp::CborMap& reply,
                                              void* /*user*/) {
    double t = (double)g_threshold_c;
    while (params.remaining() > 0) {
        const char* key = nullptr;
        size_t key_len = 0;
        if (!params.next_key(&key, &key_len)) return dcp::STATUS_DENIED;
        if (key_len == 11 && memcmp(key, "temperature", 11) == 0) {
            if (!params.read_float(&t)) return dcp::STATUS_RANGE;
        } else {
            params.skip();
        }
    }
    if (t < -40.0 || t > 80.0) return dcp::STATUS_RANGE;

    if (kind == dcp::KIND_DRY_RUN) {
        reply.add_float("would_set_threshold", t);
        return dcp::STATUS_OK;
    }

    g_threshold_c = (float)t;
    g_above_last  = false;   // re-arm edge trigger after threshold change
    return dcp::STATUS_OK;
}

static dcp::IntentBinding bindings[] = {
    { DCP_ID("read_temperature"),    handle_read_temperature,    nullptr },
    { DCP_ID("read_humidity"),       handle_read_humidity,       nullptr },
    { DCP_ID("set_alert_threshold"), handle_set_alert_threshold, nullptr },
};

void setup() {
    Serial.begin(115200);
    // Real DHT22 init goes here: dht.begin();
    static dcp::DCP instance(Serial, bindings, sizeof(bindings) / sizeof(bindings[0]));
    dcp_instance = &instance;
}

void loop() {
    if (dcp_instance) dcp_instance->poll();

    // Periodic sensor read + edge-triggered event emission.
    uint32_t now = millis();
    if ((int32_t)(now - g_next_sample_ms) < 0) return;
    g_next_sample_ms = now + SAMPLE_INTERVAL_MS;

    g_last_temp_c = read_dht_temperature_c();
    bool above_now = g_last_temp_c > g_threshold_c;

    if (above_now && !g_above_last) {
        // Crossed threshold from below — emit one event.
        uint8_t buf[48];
        dcp::CborMap m(buf, sizeof(buf));
        m.begin();
        m.add_float("temperature", (double)g_last_temp_c);
        m.add_float("threshold",   (double)g_threshold_c);
        if (dcp_instance) dcp_instance->send_event("threshold_exceeded", m);
    }
    g_above_last = above_now;
}

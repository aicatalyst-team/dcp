// Recipe: 28BYJ-48 stepper motor + ULN2003 driver.
//
// Highlights:
//   - dry_run that's GENUINELY USEFUL — the LLM can ask "what step
//     position would this move end at?" without spinning the shaft
//   - non-idempotent intent declared as such (cumulative moves)
//   - device-tracked absolute position, exposed via read_position
//
// Pair with:
//   dcp serve examples/stepper_manifest.yaml --serial COM3
//
// Hardware (when wiring real motor):
//   ULN2003 IN1 -> GPIO 14
//   ULN2003 IN2 -> GPIO 27
//   ULN2003 IN3 -> GPIO 26
//   ULN2003 IN4 -> GPIO 25
//   ULN2003 + / -  -> external 5V (do NOT power from ESP32 3.3V!)
//
// The driver function step_pulse() is stubbed for the skeleton — it
// just updates the position counter without actually energising coils.
// Replace with your stepper library or hand-rolled coil sequence.

#include "DCP.h"

constexpr int IN1 = 14;
constexpr int IN2 = 27;
constexpr int IN3 = 26;
constexpr int IN4 = 25;

constexpr int32_t STEPS_PER_REV = 4096;
constexpr uint32_t MIN_PULSE_US = 1200;   // ULN2003 + 28BYJ-48 max safe step rate

static int32_t g_position = 0;            // absolute step counter

// 4-step half-drive sequence (8 entries, half-step mode for smoother motion).
static const uint8_t HALF_STEP[8][4] = {
    {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
    {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1},
};
static uint8_t g_seq_idx = 0;

static void apply_coils(uint8_t idx) {
    digitalWrite(IN1, HALF_STEP[idx][0]);
    digitalWrite(IN2, HALF_STEP[idx][1]);
    digitalWrite(IN3, HALF_STEP[idx][2]);
    digitalWrite(IN4, HALF_STEP[idx][3]);
}

// Move one half-step in the given direction (+1 or -1).
static void step_pulse(int8_t dir, uint32_t pulse_us) {
    g_seq_idx = (uint8_t)((g_seq_idx + (dir > 0 ? 1 : 7)) & 0x07);
    apply_coils(g_seq_idx);
    delayMicroseconds(pulse_us);
}

// Move `count` whole-rev-equivalent steps. Caller has already validated.
static void do_move(int dir, int32_t count, int rpm) {
    // pulse_us per half-step: 60_000_000 us per min / (rpm * STEPS_PER_REV * 2)
    uint32_t pulse_us = 60000000UL / ((uint32_t)rpm * STEPS_PER_REV * 2UL);
    if (pulse_us < MIN_PULSE_US) pulse_us = MIN_PULSE_US;
    for (int32_t i = 0; i < count * 2; ++i) {   // 2 half-steps per full step
        step_pulse((int8_t)dir, pulse_us);
    }
    g_position += (int32_t)dir * count;
    // Release coils to avoid overheating when idle.
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

static dcp::Status handle_step(uint8_t kind,
                               dcp::CborReader& params,
                               dcp::CborMap& reply,
                               void* /*user*/) {
    int64_t direction = 0, count = 512, rpm = 8;
    while (params.remaining() > 0) {
        const char* key = nullptr;
        size_t key_len = 0;
        if (!params.next_key(&key, &key_len)) return dcp::STATUS_DENIED;
        if      (key_len == 9 && memcmp(key, "direction",  9) == 0) { if (!params.read_int(&direction)) return dcp::STATUS_RANGE; }
        else if (key_len == 5 && memcmp(key, "count",      5) == 0) { if (!params.read_int(&count))     return dcp::STATUS_RANGE; }
        else if (key_len == 9 && memcmp(key, "speed_rpm",  9) == 0) { if (!params.read_int(&rpm))       return dcp::STATUS_RANGE; }
        else { params.skip(); }
    }
    if (direction != -1 && direction != 1)  return dcp::STATUS_RANGE;
    if (count < 1 || count > 4096)          return dcp::STATUS_RANGE;
    if (rpm < 1 || rpm > 15)                return dcp::STATUS_RANGE;

    int32_t target = g_position + (int32_t)direction * (int32_t)count;
    if (kind == dcp::KIND_DRY_RUN) {
        reply.add_int("would_move_to", (int64_t)target);
        reply.add_int("from",          (int64_t)g_position);
        return dcp::STATUS_OK;
    }

    do_move((int)direction, (int32_t)count, (int)rpm);
    reply.add_int("position", (int64_t)g_position);
    return dcp::STATUS_OK;
}

static dcp::Status handle_read_position(uint8_t /*kind*/,
                                        dcp::CborReader& /*params*/,
                                        dcp::CborMap& reply,
                                        void* /*user*/) {
    reply.add_int("value", (int64_t)g_position);
    return dcp::STATUS_OK;
}

static dcp::Status handle_home(uint8_t kind,
                               dcp::CborReader& /*params*/,
                               dcp::CborMap& reply,
                               void* /*user*/) {
    if (kind == dcp::KIND_DRY_RUN) {
        reply.add_int("would_move_to", 0);
        reply.add_int("from", (int64_t)g_position);
        return dcp::STATUS_OK;
    }
    if (g_position == 0) {
        return dcp::STATUS_OK;   // already home
    }
    int dir = (g_position > 0) ? -1 : 1;
    int32_t count = (g_position > 0) ? g_position : -g_position;
    do_move(dir, count, 8);
    reply.add_int("position", (int64_t)g_position);
    return dcp::STATUS_OK;
}

static dcp::IntentBinding bindings[] = {
    { DCP_ID("step"),          handle_step,          nullptr },
    { DCP_ID("read_position"), handle_read_position, nullptr },
    { DCP_ID("home"),          handle_home,          nullptr },
};

static dcp::DCP* dcp_instance = nullptr;

void setup() {
    Serial.begin(115200);
    pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
    static dcp::DCP instance(Serial, bindings, sizeof(bindings) / sizeof(bindings[0]));
    dcp_instance = &instance;
}

void loop() {
    if (dcp_instance) dcp_instance->poll();
}

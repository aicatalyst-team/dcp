// Recipe: smart door lock — capability + dry_run as REAL safety story.
//
// What this demonstrates that lamp/relay don't:
//   1. A capability name that signals seriousness (lock.admin, not
//      lock.write) — the LLM tool schema surfaces this, and a human
//      reviewing the manifest immediately sees "this is dangerous."
//   2. dry_run as actual security: an LLM can be ALLOWED to call unlock
//      with __dry_run__=true (cheap, reversible — just returns "would
//      transition locked -> unlocked"), but the REAL unlock requires an
//      explicit capability token that's typically minted out-of-band
//      with very short TTL.
//   3. String-typed return values (locked / unlocked).
//   4. State-change events independent of who triggered them — so even
//      a physical manual override gets observed by anyone with lock.read.
//
// Pair with:
//   dcp serve examples/door_lock_manifest.yaml --serial COM3 --grant lock.read
// (deliberately granting ONLY read by default; admin requires a token.)
//
// Hardware (stubbed for the skeleton — actuator driver is a no-op):
//   - SG90 servo signal on ACTUATOR_PIN, external 5V power for servo
//   - Or: 12V solenoid lock via MOSFET / relay on ACTUATOR_PIN
//
// Replace set_actuator() below with your servo PWM or relay drive logic.

#include "DCP.h"

constexpr int  ACTUATOR_PIN = 13;

// Internal state — string-typed at the API boundary, bool internally.
static bool g_locked = true;
static dcp::DCP* dcp_instance = nullptr;

// ---- Actuator driver stub --------------------------------------------------
// Replace this with your real driver (servo writeMicroseconds, relay digital,
// solenoid PWM, etc). The skeleton version just drives a pin HIGH or LOW so
// you can verify wiring with an LED.
static void set_actuator(bool locked) {
    digitalWrite(ACTUATOR_PIN, locked ? LOW : HIGH);
}
// ---------------------------------------------------------------------------

static void emit_state_changed(const char* from, const char* to) {
    uint8_t buf[48];
    dcp::CborMap m(buf, sizeof(buf));
    m.begin();
    m.add_string("from", from);
    m.add_string("to",   to);
    if (dcp_instance) dcp_instance->send_event("state_changed", m);
}

// Apply a state transition. Returns true if the state actually changed,
// false if it was already in the target state.
static bool transition_to(bool new_locked) {
    if (g_locked == new_locked) return false;
    const char* from_str = g_locked   ? "locked" : "unlocked";
    const char* to_str   = new_locked ? "locked" : "unlocked";
    g_locked = new_locked;
    set_actuator(g_locked);
    emit_state_changed(from_str, to_str);
    return true;
}

static dcp::Status handle_unlock(uint8_t kind,
                                 dcp::CborReader& /*params*/,
                                 dcp::CborMap& reply,
                                 void* /*user*/) {
    if (kind == dcp::KIND_DRY_RUN) {
        reply.add_string("would_transition_from", g_locked ? "locked" : "unlocked");
        reply.add_string("would_transition_to",   "unlocked");
        return dcp::STATUS_OK;
    }
    transition_to(false);     // unlock — emits state_changed if actually changed
    return dcp::STATUS_OK;
}

static dcp::Status handle_lock(uint8_t kind,
                               dcp::CborReader& /*params*/,
                               dcp::CborMap& reply,
                               void* /*user*/) {
    if (kind == dcp::KIND_DRY_RUN) {
        reply.add_string("would_transition_from", g_locked ? "locked" : "unlocked");
        reply.add_string("would_transition_to",   "locked");
        return dcp::STATUS_OK;
    }
    transition_to(true);
    return dcp::STATUS_OK;
}

static dcp::Status handle_read_state(uint8_t /*kind*/,
                                     dcp::CborReader& /*params*/,
                                     dcp::CborMap& reply,
                                     void* /*user*/) {
    reply.add_string("value", g_locked ? "locked" : "unlocked");
    return dcp::STATUS_OK;
}

static dcp::IntentBinding bindings[] = {
    { DCP_ID("unlock"),     handle_unlock,     nullptr },
    { DCP_ID("lock"),       handle_lock,       nullptr },
    { DCP_ID("read_state"), handle_read_state, nullptr },
};

void setup() {
    Serial.begin(115200);
    pinMode(ACTUATOR_PIN, OUTPUT);
    set_actuator(g_locked);   // power-on default = locked (fail safe)
    static dcp::DCP instance(Serial, bindings, sizeof(bindings) / sizeof(bindings[0]));
    dcp_instance = &instance;
}

void loop() {
    if (dcp_instance) dcp_instance->poll();
    // A real product would also poll a physical key sensor here and call
    // transition_to() to fire state_changed for manual overrides.
}

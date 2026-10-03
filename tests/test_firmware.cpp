// Compile the production sketch itself; only Arduino hardware primitives are fake.
#include <cassert>
#include <iostream>
#include "../firmware/child_buzzer/child_buzzer.ino"

void reset() {
    fake::reset();
    for (int i = 0; i < 7; ++i) {
        keyHeld[i] = lastReading[i] = false;
        lastChangeMs[i] = pressOrder[i] = 0;
    }
    pressCounter = 1; lastFreqWritten = -1;
    vibratoOn = false; vibratoPhase = 0; lastVibratoMs = 0;
    comboActive = comboFired = false; comboStartMs = 0;
}
void tick(uint32_t now) { fake::now = now; loop(); }
void setKey(int key, bool held) { fake::pins[KEY_PINS[key]] = held ? LOW : HIGH; }

void test_debounce_priority_and_release() {
    reset(); setup();
    for (auto pin : KEY_PINS) assert(fake::modes[pin] == INPUT_PULLUP);
    assert(fake::modes[BUZZER_PIN] == OUTPUT);
    setKey(1, true); tick(100); tick(104); assert(activeNote(false) == -1);
    tick(105); assert(activeNote(false) == 1); assert(fake::audio.back() == NOTE_HZ[1]);
    auto writes = fake::audio.size(); tick(106); assert(fake::audio.size() == writes);
    setKey(3, true); tick(110); tick(115); assert(activeNote(false) == 3);
    setKey(3, false); tick(120); tick(125); assert(activeNote(false) == 1);
    setKey(1, false); tick(130); tick(135); assert(activeNote(false) == -1);
    assert(fake::audio.back() == -1);
}
void test_bounce_and_clock_rollover() {
    reset(); setKey(2, true); tick(UINT32_MAX - 2);
    tick(1); assert(!keyHeld[2]); tick(2); assert(keyHeld[2]);
    reset(); setKey(2, true); tick(10); setKey(2, false); tick(13);
    setKey(2, true); tick(16); tick(20); assert(!keyHeld[2]); tick(21); assert(keyHeld[2]);
}
void test_combo_threshold_once_rearm_and_rollover() {
    reset(); keyHeld[0] = keyHeld[6] = true;
    pressOrder[0] = 2; pressOrder[6] = 3;
    uint32_t start = UINT32_MAX - 300;
    updateComboToggle(start); assert(activeNote(true) == -1);
    updateComboToggle(start + COMBO_HOLD_MS - 1); assert(!vibratoOn);
    updateComboToggle(start + COMBO_HOLD_MS); assert(vibratoOn);
    assert(fake::audio.size() == 4); assert(lastFreqWritten == -1);
    updateComboToggle(start + 2000); assert(vibratoOn); assert(fake::audio.size() == 4);
    keyHeld[0] = false; updateComboToggle(2000); keyHeld[0] = true;
    updateComboToggle(2001); updateComboToggle(2601); assert(!vibratoOn);
}
void test_octaves_and_vibrato_bounds() {
    reset();
    for (int raw = 0; raw <= 1024; ++raw) {
        fake::knob = raw;
        assert(readOctaveBand() == std::min(raw / 256, 3));
    }
    for (int key = 0; key < 7; ++key) for (int band = 0; band < 4; ++band) {
        auto base = noteFrequency(key, band); assert(base == NOTE_HZ[key] * (1 << band));
        for (vibratoPhase = 0; vibratoPhase < VIBRATO_STEPS; ++vibratoPhase) {
            auto hz = applyVibrato(base);
            assert(hz >= base * 960 / 1000 && hz <= base * 1040 / 1000);
        }
    }
}
int main() {
    test_debounce_priority_and_release(); test_bounce_and_clock_rollover();
    test_combo_threshold_once_rearm_and_rollover(); test_octaves_and_vibrato_bounds();
    std::cout << "4 actual-sketch test groups passed\n";
}

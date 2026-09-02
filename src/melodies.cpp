#include "melodies.h"
#include "persistence.h"
#include <esp_task_wdt.h>

const MelodyInfo AVAILABLE_MELODIES[MELODY_COUNT] = {
    { MELODY_SIMPLE_CHIRP,      "Simple Chirp (Default)" },
    { MELODY_DUAL_ALERT,        "Dual Tone Warning" },
    { MELODY_SMOKE_ON_WATER,    "Rock: Smoke on the Water (Deep Purple)" },
    { MELODY_SEVEN_NATION_ARMY, "Rock: Seven Nation Army (White Stripes)" },
    { MELODY_BACK_IN_BLACK,     "Rock: Back in Black (AC/DC)" },
    { MELODY_FINAL_COUNTDOWN,   "Rock: The Final Countdown (Europe)" },
    { MELODY_SWEET_CHILD,       "Rock: Sweet Child O' Mine (Guns N' Roses)" },
    { MELODY_EYE_OF_THE_TIGER,  "Rock: Eye of the Tiger (Survivor)" },
    { MELODY_SUPER_MARIO,       "Theme: Super Mario Bros" },
    { MELODY_IMPERIAL_MARCH,    "Theme: Star Wars Imperial March" }
};

struct Note {
    uint16_t freq;
    uint16_t durationMs;
};

static const Note MELODY_DATA_CHIRP[] = {
    {NOTE_C5, 80}, {NOTE_E5, 80}, {NOTE_G5, 120}
};

static const Note MELODY_DATA_ALERT[] = {
    {NOTE_A5, 150}, {NOTE_REST, 50}, {NOTE_A5, 150}, {NOTE_REST, 50}, {NOTE_A5, 300}
};

static const Note MELODY_DATA_SMOKE[] = {
    {NOTE_D4, 250}, {NOTE_F4, 250}, {NOTE_G4, 375}, {NOTE_REST, 125},
    {NOTE_D4, 250}, {NOTE_F4, 250}, {NOTE_GS4, 125}, {NOTE_G4, 375}, {NOTE_REST, 125},
    {NOTE_D4, 250}, {NOTE_F4, 250}, {NOTE_G4, 375}, {NOTE_F4, 250}, {NOTE_D4, 500}
};

static const Note MELODY_DATA_SEVEN_NATION[] = {
    {NOTE_E4, 400}, {NOTE_E4, 200}, {NOTE_G4, 200}, {NOTE_E4, 200}, {NOTE_D4, 200}, {NOTE_C4, 400}, {NOTE_B4, 400}
};

static const Note MELODY_DATA_BACK_IN_BLACK[] = {
    {NOTE_E5, 150}, {NOTE_D5, 150}, {NOTE_C5, 150}, {NOTE_REST, 100},
    {NOTE_A4, 200}, {NOTE_C5, 200}, {NOTE_A4, 200}, {NOTE_G4, 200}
};

static const Note MELODY_DATA_FINAL_COUNTDOWN[] = {
    {NOTE_B4, 150}, {NOTE_A4, 150}, {NOTE_B4, 300}, {NOTE_E4, 600}, {NOTE_REST, 150},
    {NOTE_C5, 150}, {NOTE_B4, 150}, {NOTE_C5, 300}, {NOTE_B4, 300}, {NOTE_A4, 600}
};

static const Note MELODY_DATA_SWEET_CHILD[] = {
    {NOTE_D4, 150}, {NOTE_D5, 150}, {NOTE_A4, 150}, {NOTE_G4, 150},
    {NOTE_G5, 150}, {NOTE_A4, 150}, {NOTE_FS5, 150}, {NOTE_A4, 150}
};

static const Note MELODY_DATA_EYE_OF_TIGER[] = {
    {NOTE_C5, 300}, {NOTE_REST, 100}, {NOTE_C5, 150}, {NOTE_AS4, 150}, {NOTE_C5, 300}, {NOTE_REST, 100},
    {NOTE_C5, 150}, {NOTE_AS4, 150}, {NOTE_C5, 300}, {NOTE_REST, 100}, {NOTE_G4, 400}
};

static const Note MELODY_DATA_MARIO[] = {
    {NOTE_E5, 125}, {NOTE_E5, 125}, {NOTE_REST, 125}, {NOTE_E5, 125},
    {NOTE_REST, 125}, {NOTE_C5, 125}, {NOTE_E5, 250}, {NOTE_G5, 250}
};

static const Note MELODY_DATA_IMPERIAL[] = {
    {NOTE_A4, 350}, {NOTE_A4, 350}, {NOTE_A4, 350}, {NOTE_F4, 250}, {NOTE_C5, 125}, {NOTE_A4, 350}
};

static void playNoteSequence(const Note* notes, size_t count) {
    bool toneActive = false;

    for (size_t i = 0; i < count; i++) {
        if (notes[i].freq == NOTE_REST) {
            if (toneActive) {
                noTone(PIN_BUZZER);
                toneActive = false;
            }
        } else {
            tone(PIN_BUZZER, notes[i].freq);
            toneActive = true;
        }

        // WDT-safe delay loop for note duration
        unsigned long startMs = millis();
        while (millis() - startMs < notes[i].durationMs) {
            esp_task_wdt_reset();
            delay(1);
        }

        if (toneActive) {
            noTone(PIN_BUZZER);
            toneActive = false;
        }

        // Inter-note gap delay loop
        startMs = millis();
        while (millis() - startMs < 20) {
            esp_task_wdt_reset();
            delay(1);
        }
    }

    if (toneActive) {
        noTone(PIN_BUZZER);
    }
}

void playMelody(MelodyID id) {
    switch (id) {
        case MELODY_SIMPLE_CHIRP:
            playNoteSequence(MELODY_DATA_CHIRP, sizeof(MELODY_DATA_CHIRP) / sizeof(Note));
            break;
        case MELODY_DUAL_ALERT:
            playNoteSequence(MELODY_DATA_ALERT, sizeof(MELODY_DATA_ALERT) / sizeof(Note));
            break;
        case MELODY_SMOKE_ON_WATER:
            playNoteSequence(MELODY_DATA_SMOKE, sizeof(MELODY_DATA_SMOKE) / sizeof(Note));
            break;
        case MELODY_SEVEN_NATION_ARMY:
            playNoteSequence(MELODY_DATA_SEVEN_NATION, sizeof(MELODY_DATA_SEVEN_NATION) / sizeof(Note));
            break;
        case MELODY_BACK_IN_BLACK:
            playNoteSequence(MELODY_DATA_BACK_IN_BLACK, sizeof(MELODY_DATA_BACK_IN_BLACK) / sizeof(Note));
            break;
        case MELODY_FINAL_COUNTDOWN:
            playNoteSequence(MELODY_DATA_FINAL_COUNTDOWN, sizeof(MELODY_DATA_FINAL_COUNTDOWN) / sizeof(Note));
            break;
        case MELODY_SWEET_CHILD:
            playNoteSequence(MELODY_DATA_SWEET_CHILD, sizeof(MELODY_DATA_SWEET_CHILD) / sizeof(Note));
            break;
        case MELODY_EYE_OF_THE_TIGER:
            playNoteSequence(MELODY_DATA_EYE_OF_TIGER, sizeof(MELODY_DATA_EYE_OF_TIGER) / sizeof(Note));
            break;
        case MELODY_SUPER_MARIO:
            playNoteSequence(MELODY_DATA_MARIO, sizeof(MELODY_DATA_MARIO) / sizeof(Note));
            break;
        case MELODY_IMPERIAL_MARCH:
            playNoteSequence(MELODY_DATA_IMPERIAL, sizeof(MELODY_DATA_IMPERIAL) / sizeof(Note));
            break;
        default:
            playNoteSequence(MELODY_DATA_CHIRP, sizeof(MELODY_DATA_CHIRP) / sizeof(Note));
            break;
    }
}

void playStartupMelodyConfigured() {
    playMelody((MelodyID)sysStats.startupMelody);
}

void playUpperLimitMelodyConfigured() {
    playMelody((MelodyID)sysStats.upperLimitMelody);
}

void playLowerLimitMelodyConfigured() {
    playMelody((MelodyID)sysStats.lowerLimitMelody);
}
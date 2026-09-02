#ifndef MELODIES_H
#define MELODIES_H

#include <Arduino.h>

#define PIN_BUZZER       22

// Musical Note Frequencies (Hz)
#define NOTE_REST 0
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047

enum MelodyID {
    MELODY_SIMPLE_CHIRP = 0,
    MELODY_DUAL_ALERT,
    MELODY_SMOKE_ON_WATER,   // Deep Purple
    MELODY_SEVEN_NATION_ARMY, // The White Stripes
    MELODY_BACK_IN_BLACK,    // AC/DC
    MELODY_FINAL_COUNTDOWN,  // Europe
    MELODY_SWEET_CHILD,      // Guns N' Roses
    MELODY_EYE_OF_THE_TIGER, // Survivor
    MELODY_SUPER_MARIO,
    MELODY_IMPERIAL_MARCH,
    MELODY_COUNT
};

struct MelodyInfo {
    MelodyID id;
    const char* name;
};

extern const MelodyInfo AVAILABLE_MELODIES[MELODY_COUNT];

void playMelody(MelodyID id);
void playStartupMelodyConfigured();
void playUpperLimitMelodyConfigured();
void playLowerLimitMelodyConfigured();

#endif // MELODIES_H
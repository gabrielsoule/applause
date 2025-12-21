#pragma once
#include <clap/events.h>
#include <cmath>

#include "applause/util/DebugHelpers.h"

namespace applause {
/**
 * Represents a CLAP note with all its expressive dimensions.
 *
 * The CLAP note spec is distinct to the CLAP protocol (and CLAP hosts); it can be interpreted as a
 * modern MIDI-like protocol that is, in a sense, a supserset of both MIDI 1.0 and MPE.
 *
 * As such, Applause wraps and translates all incoming note events into the CLAP note format. As a developer, you can
 * design your MIDI handling with respect to the CLAP note protocol. Applause will handle the rest.
 *
 * As a bonus, you get MPE "for free"; no special MPE handling is required.
 *
 */
struct Note {
    enum Expression {
        Volume = 0,
        Pan = 1,
        Tuning = 2,
        Vibrato = 3,
        Dynamics = 4,     // MIDI CC 11 - Expression/Dynamics
        Timbre = 5,      // Brightness/Y-axis in MPE
        Pressure = 6,    // Aftertouch/Z-axis in MPE
    };

    // Immutable identifiers (set at note on, never change)
    int32_t note_id = -1; // Host-provided unique ID, or -1 if not specified
    int16_t port_index = 0; // CLAP port index
    int16_t channel = 0; // MIDI channel 0-15
    int16_t key = 0; // MIDI note number 0-127 (60 = Middle C)

    // Immutable velocities. These are directly translated from original MIDI, and never change over a note's lifetime.
    double note_on_velocity = 0.0; // 0..1, set at note on
    double note_off_velocity = 0.0; // 0..1, set at note off

    // The CLAP spec defines several note expressions, which can change during the lifetime of a note.
    // Some of these are similar to MPE expressions, and map neatly tom those parameters.
    // Some others are new, and have limited host support!
    // These can be modulated during the note's lifetime

    // Each CLAP note has a volume that can be updated while a note is active (unlike velocity)
    // 0 < volume <= 4 (1.0 = unity, 2.0 = +6dB, logarithmic)
    double volume = 1.0;

    // 0..1 (0=left, 0.5=center, 1=right)
    double pan = 0.5;

    // -120..+120 semitones relative to key
    // This changes when the tuning is modulated, either via A MPE "slide" or through a MIDI 1.0 pitch bend wheel event.
    // In the latter case, a "wildcard" message is sent from the host to Applause, indicating that all notes
    // should be tuned by the same amount. Meanwhile, MPE notes will be tuned individually.
    double tuning = 0.0;

    // 0..1
    double vibrato = 0.0;

    // 0..1 (like MIDI CC 11)
    // Old school expression, not used by MPE (despite the name), but it's part of the CLAP note spec
    // Often used to shape amplitude via a foot pedal or something similar.
    double expression = 0.0;

    // 0..1 (timbre/Y-axis in MPE)
    double brightness = 0.5;

    // 0..1 (aftertouch/Z-axis in MPE)
    double pressure = 0.0;

    /**
     * Create a Note from a CLAP note on event.
     */
    static Note fromNoteOn(const clap_event_note_t* event) {
        Note note;
        note.note_id = event->note_id;
        note.port_index = event->port_index;
        note.channel = event->channel;
        note.key = event->key;
        note.note_on_velocity = event->velocity;
        return note;
    }

    /**
     * Update the note off velocity from a CLAP note off event.
     */
    void setNoteOff(const clap_event_note_t* event) {
        note_off_velocity = event->velocity;
    }

    /**
     * Calculate the frequency in Hz, accounting for the base key and tuning expression.
     * @param a4_frequency The reference frequency for A4 (default 440 Hz)
     * @return Frequency in Hz
     */
    double getFrequency(double a4_frequency = 440.0) const {
        return a4_frequency * std::pow(2.0, (key - 69.0 + tuning) / 12.0);
    }

    /**
     * Check if this note matches the given identifiers using CLAP wildcard rules.
     * A value of -1 in any field means "wildcard" (matches any value).
     */
    bool matches(int16_t event_key, int32_t event_note_id = -1,
                 int16_t event_port = -1, int16_t event_channel = -1) const {
        bool key_match = (event_key == -1 || event_key == key);
        bool id_match = (event_note_id == -1 || event_note_id == note_id);
        bool port_match = (event_port == -1 || event_port == port_index);
        bool channel_match = (event_channel == -1 || event_channel == channel);
        return key_match && id_match && port_match && channel_match;
    }

    /**
     * Apply a CLAP note expression to this note.
     */
    void applyExpression(Expression expression_id, double value) {
        switch (expression_id) {
            case Expression::Volume:
                volume = value;
                break;
            case Expression::Pan:
                pan = value;
                break;
            case Expression::Tuning:
                tuning = value;
                break;
            case Expression::Vibrato:
                vibrato = value;
                break;
            case Expression::Dynamics:
                expression = value;
                break;
            case Expression::Timbre:
                brightness = value;
                break;
            case Expression::Pressure:
                pressure = value;
                break;
            default:
                ASSERT_FALSE("Invalid expression ID ({}); this should never happen!", static_cast<int>(expression_id));
                break;
        }
    }

    /**
     * Get the volume in decibels.
     * CLAP volume is linear amplitude where 1.0 = unity.
     */
    double getVolumeDb() const {
        if (volume <= 0.0) return -100.0; // Effectively -infinity
        return 20.0 * std::log10(volume);
    }
};
} // namespace applause
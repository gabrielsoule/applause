#pragma once

#include "applause/core/PluginBase.h"
#include "applause/extensions/NotePortsExtension.h"
#include "applause/extensions/AudioPortsExtension.h"
#include "applause/extensions/StateExtension.h"
#include <string>

/**
 * A utility plugin that prints info about all MIDI events to the terminal.
 * You can use this plugin to learn how Applause plugins are sent MIDI through the CLAP interface,
 * in case you want to build your own custom MIDI handling or synthesizer code.
 * It also shows how MIDI information can be parsed from CLAP event structs.
 *
 * Note: the log calls are turned to no-ops when compiled for release, so build and run this as a debug build!
 */
class ExampleMIDIPrinterPlugin : public applause::PluginBase {
public:
    explicit ExampleMIDIPrinterPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host);
    ~ExampleMIDIPrinterPlugin() override = default;
    
    // Plugin lifecycle
    bool init() noexcept override;
    void destroy() noexcept override;
    bool activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept override;
    void deactivate() noexcept override;
    
    // Audio processing
    clap_process_status process(const clap_process_t* process) noexcept override;
    
private:
    // Extensions
    applause::NotePortsExtension note_ports_;
    applause::AudioPortsExtension audio_ports_;
    applause::StateExtension state_;
    
    void printNoteEvent(const clap_event_note_t* event, const char* event_name);
    void printNoteExpression(const clap_event_note_expression_t* event);
    void printMidiEvent(const clap_event_midi_t* event);
    void printMidi2Event(const clap_event_midi2_t* event);
    void printMidiSysexEvent(const clap_event_midi_sysex_t* event);
    
    std::string decodeMidiStatus(uint8_t status);
    std::string getNoteNameFromKey(int key);
    std::string getExpressionName(int32_t expression_id);
    
    uint64_t event_count_;
};

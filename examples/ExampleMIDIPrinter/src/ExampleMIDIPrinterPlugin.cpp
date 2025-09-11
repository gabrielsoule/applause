#include "ExampleMIDIPrinterPlugin.h"
#include <cstring>
#include <iomanip>

ExampleMIDIPrinterPlugin::ExampleMIDIPrinterPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host),
      note_ports_(host),
      event_count_(0)
{
    LOG_INFO("ExampleMIDIPrinter constructor");

    // Configure note ports - universal input that accepts all dialects
    note_ports_.addInput(applause::NotePortConfig::universal("Note Input"));

    // Query what the host supports
    uint32_t host_dialects = note_ports_.getHostSupportedDialects();
    LOG_INFO("Host supported note dialects:");
    if (host_dialects & CLAP_NOTE_DIALECT_CLAP)
        LOG_INFO("  - CLAP (native)");
    if (host_dialects & CLAP_NOTE_DIALECT_MIDI)
        LOG_INFO("  - MIDI 1.0");
    if (host_dialects & CLAP_NOTE_DIALECT_MIDI_MPE)
        LOG_INFO("  - MIDI MPE");
    if (host_dialects & CLAP_NOTE_DIALECT_MIDI2)
        LOG_INFO("  - MIDI 2.0");

    // Provide a simple stereo audio output so VST3 hosts can configure buses
    audio_ports_.addOutput(applause::AudioPortConfig::mainStereo("Main Out"));

    // Register extensions
    registerExtension(note_ports_);
    registerExtension(audio_ports_);
    registerExtension(state_);
}

bool ExampleMIDIPrinterPlugin::init() noexcept
{
    return true;
}

void ExampleMIDIPrinterPlugin::destroy() noexcept
{
    LOG_INFO("Total events processed: {}", event_count_);
}

bool ExampleMIDIPrinterPlugin::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept
{
    LOG_INFO("Activating MIDI printer...");
    LOG_INFO("  Sample rate: {} Hz", sampleRate);
    LOG_INFO("  Frame count range: {} - {}", minFrameCount, maxFrameCount);
    event_count_ = 0;
    return true;
}

void ExampleMIDIPrinterPlugin::deactivate() noexcept
{
    LOG_INFO("Events processed in this session: {}", event_count_);
}

clap_process_status ExampleMIDIPrinterPlugin::process(const clap_process_t* process) noexcept
{
    // Emit silence if we have an audio output bus to keep hosts happy
    if (process->audio_outputs_count > 0)
    {
        const clap_audio_buffer_t* out = &process->audio_outputs[0];
        for (uint32_t ch = 0; ch < out->channel_count; ++ch)
        {
            if (out->data32[ch])
                std::memset(out->data32[ch], 0, sizeof(float) * process->frames_count);
        }
    }

    if (process->in_events)
    {
        uint32_t event_count = process->in_events->size(process->in_events);

        for (uint32_t i = 0; i < event_count; ++i)
        {
            const clap_event_header_t* header = process->in_events->get(process->in_events, i);
            if (!header) continue;

            event_count_++;

            // Print common header info
            LOG_INFO("=== Event #{} ===", event_count_);
            LOG_INFO("  Time: sample {} (block offset)", header->time);
            LOG_INFO("  Size: {} bytes", header->size);
            LOG_INFO("  Space: {}", header->space_id);

            // Process based on event type
            if (header->space_id == CLAP_CORE_EVENT_SPACE_ID)
            {
                switch (header->type)
                {
                case CLAP_EVENT_NOTE_ON:
                    printNoteEvent(reinterpret_cast<const clap_event_note_t*>(header), "NOTE_ON");
                    break;
                case CLAP_EVENT_NOTE_OFF:
                    printNoteEvent(reinterpret_cast<const clap_event_note_t*>(header), "NOTE_OFF");
                    break;
                case CLAP_EVENT_NOTE_CHOKE:
                    printNoteEvent(reinterpret_cast<const clap_event_note_t*>(header), "NOTE_CHOKE");
                    break;
                case CLAP_EVENT_NOTE_END:
                    printNoteEvent(reinterpret_cast<const clap_event_note_t*>(header), "NOTE_END");
                    break;
                case CLAP_EVENT_NOTE_EXPRESSION:
                    printNoteExpression(reinterpret_cast<const clap_event_note_expression_t*>(header));
                    break;
                case CLAP_EVENT_MIDI:
                    printMidiEvent(reinterpret_cast<const clap_event_midi_t*>(header));
                    break;
                case CLAP_EVENT_MIDI2:
                    printMidi2Event(reinterpret_cast<const clap_event_midi2_t*>(header));
                    break;
                case CLAP_EVENT_MIDI_SYSEX:
                    printMidiSysexEvent(reinterpret_cast<const clap_event_midi_sysex_t*>(header));
                    break;
                default:
                    LOG_INFO("  Type: Unknown core event ({})", header->type);
                    break;
                }
            }
            else
            {
                LOG_INFO("  Type: Non-core event space");
            }
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

void ExampleMIDIPrinterPlugin::printNoteEvent(const clap_event_note_t* event, const char* event_name)
{
    LOG_INFO("  Type: CLAP {} Event", event_name);
    LOG_INFO("  Port: {}", event->port_index >= 0 ? std::to_string(event->port_index) : "wildcard");
    LOG_INFO("  Channel: {}", event->channel >= 0 ? std::to_string(event->channel) : "wildcard");
    LOG_INFO("  Key: {} {}",
             event->key >= 0 ? std::to_string(event->key) : "wildcard",
             event->key >= 0 ? "(" + getNoteNameFromKey(event->key) + ")" : "");
    LOG_INFO("  Velocity: {:.3f}", event->velocity);
    LOG_INFO("  Note ID: {}", event->note_id >= 0 ? std::to_string(event->note_id) : "unspecified");
}

void ExampleMIDIPrinterPlugin::printNoteExpression(const clap_event_note_expression_t* event)
{
    LOG_INFO("  Type: CLAP NOTE_EXPRESSION Event");
    LOG_INFO("  Expression: {} ({})", event->expression_id, getExpressionName(event->expression_id));
    LOG_INFO("  Port: {}", event->port_index >= 0 ? std::to_string(event->port_index) : "wildcard");
    LOG_INFO("  Channel: {}", event->channel >= 0 ? std::to_string(event->channel) : "wildcard");
    LOG_INFO("  Key: {} {}",
             event->key >= 0 ? std::to_string(event->key) : "wildcard",
             event->key >= 0 ? "(" + getNoteNameFromKey(event->key) + ")" : "");
    LOG_INFO("  Note ID: {}", event->note_id >= 0 ? std::to_string(event->note_id) : "wildcard");
    LOG_INFO("  Value: {:.6f}", event->value);

    // Provide context-specific interpretation
    switch (event->expression_id)
    {
    case CLAP_NOTE_EXPRESSION_VOLUME:
        LOG_INFO("    (Volume: {:.1f} dB)", 20.0 * std::log10(event->value));
        break;
    case CLAP_NOTE_EXPRESSION_PAN:
        LOG_INFO("    (Pan: {:.0f}%% {})",
                 std::abs(event->value - 0.5) * 200,
                 event->value < 0.5 ? "Left" : (event->value > 0.5 ? "Right" : "Center"));
        break;
    case CLAP_NOTE_EXPRESSION_TUNING:
        LOG_INFO("    (Tuning: {:.2f} cents)", event->value * 100);
        break;
    }
}

void ExampleMIDIPrinterPlugin::printMidiEvent(const clap_event_midi_t* event)
{
    LOG_INFO("  Type: MIDI 1.0 Event");
    LOG_INFO("  Port: {}", event->port_index);

    // Print raw bytes
    LOG_INFO("  Raw bytes: {:02X} {:02X} {:02X}",
             event->data[0], event->data[1], event->data[2]);

    // Decode the MIDI message
    uint8_t status = event->data[0] & 0xF0;
    uint8_t channel = event->data[0] & 0x0F;

    LOG_INFO("  Channel: {}", channel + 1); // MIDI channels are 1-based for display
    LOG_INFO("  Status: {}", decodeMidiStatus(status));

    switch (status)
    {
    case 0x80: // Note Off
    case 0x90: // Note On
        LOG_INFO("  Key: {} ({})", event->data[1], getNoteNameFromKey(event->data[1]));
        LOG_INFO("  Velocity: {}", event->data[2]);
        if (status == 0x90 && event->data[2] == 0)
        {
            LOG_INFO("  (Note: Velocity 0 - often treated as Note Off)");
        }
        break;
    case 0xA0: // Polyphonic Aftertouch
        LOG_INFO("  Key: {} ({})", event->data[1], getNoteNameFromKey(event->data[1]));
        LOG_INFO("  Pressure: {}", event->data[2]);
        break;
    case 0xB0: // Control Change
        LOG_INFO("  Controller: {}", event->data[1]);
        LOG_INFO("  Value: {}", event->data[2]);
        break;
    case 0xC0: // Program Change
        LOG_INFO("  Program: {}", event->data[1]);
        break;
    case 0xD0: // Channel Aftertouch
        LOG_INFO("  Pressure: {}", event->data[1]);
        break;
    case 0xE0: // Pitch Bend
        {
            int bend = (event->data[2] << 7) | event->data[1];
            LOG_INFO("  Bend: {} (centered at 8192)", bend);
        }
        break;
    }
}

void ExampleMIDIPrinterPlugin::printMidi2Event(const clap_event_midi2_t* event)
{
    LOG_INFO("  Type: MIDI 2.0 Event");
    LOG_INFO("  Port: {}", event->port_index);

    // Print raw data
    LOG_INFO("  Raw data: {:08X} {:08X} {:08X} {:08X}",
             event->data[0], event->data[1], event->data[2], event->data[3]);

    // Decode message type (first 4 bits of first word)
    uint8_t message_type = (event->data[0] >> 28) & 0x0F;
    uint8_t group = (event->data[0] >> 24) & 0x0F;

    LOG_INFO("  Message Type: 0x{:X}", message_type);
    LOG_INFO("  Group: {}", group);

    // Basic decoding of common MIDI 2.0 message types
    switch (message_type)
    {
    case 0x2: // MIDI 1.0 Channel Voice
        LOG_INFO("  (MIDI 1.0 Channel Voice Message)");
        break;
    case 0x3: // 64-bit Data Message
        LOG_INFO("  (64-bit Data Message)");
        break;
    case 0x4: // MIDI 2.0 Channel Voice
        LOG_INFO("  (MIDI 2.0 Channel Voice Message)");
        {
            uint8_t status = (event->data[0] >> 16) & 0xFF;
            uint8_t channel = (event->data[0] >> 8) & 0x0F;
            LOG_INFO("  Channel: {}", channel + 1);
            LOG_INFO("  Status: 0x{:02X}", status);
        }
        break;
    case 0x5: // 128-bit Data Message
        LOG_INFO("  (128-bit Data Message)");
        break;
    }
}

void ExampleMIDIPrinterPlugin::printMidiSysexEvent(const clap_event_midi_sysex_t* event)
{
    LOG_INFO("  Type: MIDI SysEx Event");
    LOG_INFO("  Port: {}", event->port_index);
    LOG_INFO("  Size: {} bytes", event->size);

    // Print first few bytes of sysex data
    if (event->buffer && event->size > 0)
    {
        std::stringstream ss;
        size_t bytes_to_print = std::min(event->size, 16u);
        for (size_t i = 0; i < bytes_to_print; ++i)
        {
            ss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(event->buffer[i]) << " ";
        }
        if (event->size > 16)
        {
            ss << "... (" << (event->size - 16) << " more bytes)";
        }
        LOG_INFO("  Data: {}", ss.str());
    }
}

std::string ExampleMIDIPrinterPlugin::decodeMidiStatus(uint8_t status)
{
    switch (status)
    {
    case 0x80: return "Note Off";
    case 0x90: return "Note On";
    case 0xA0: return "Polyphonic Aftertouch";
    case 0xB0: return "Control Change";
    case 0xC0: return "Program Change";
    case 0xD0: return "Channel Aftertouch";
    case 0xE0: return "Pitch Bend";
    case 0xF0: return "System";
    default: return "Unknown";
    }
}

std::string ExampleMIDIPrinterPlugin::getNoteNameFromKey(int key)
{
    if (key < 0 || key > 127) return "Invalid";

    const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (key / 12) - 1;
    int note = key % 12;

    return std::string(note_names[note]) + std::to_string(octave);
}

std::string ExampleMIDIPrinterPlugin::getExpressionName(int32_t expression_id)
{
    switch (expression_id)
    {
    case CLAP_NOTE_EXPRESSION_VOLUME: return "Volume";
    case CLAP_NOTE_EXPRESSION_PAN: return "Pan";
    case CLAP_NOTE_EXPRESSION_TUNING: return "Tuning";
    case CLAP_NOTE_EXPRESSION_VIBRATO: return "Vibrato";
    case CLAP_NOTE_EXPRESSION_EXPRESSION: return "Expression";
    case CLAP_NOTE_EXPRESSION_BRIGHTNESS: return "Brightness";
    case CLAP_NOTE_EXPRESSION_PRESSURE: return "Pressure";
    default: return "Unknown";
    }
}

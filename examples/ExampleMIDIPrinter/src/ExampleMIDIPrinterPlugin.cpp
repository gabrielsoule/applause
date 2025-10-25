#include "ExampleMIDIPrinterPlugin.h"
#include <cstring>
#include <iomanip>

ExampleMIDIPrinterPlugin::ExampleMIDIPrinterPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host),
      note_ports_(),
      event_count_(0)
{
    LOG_INFO("ExampleMIDIPrinter constructor");

    note_ports_.addInput(applause::NotePortConfig::universal("Note Input"));

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

bool ExampleMIDIPrinterPlugin::activate(const applause::ProcessInfo& info) noexcept
{
    LOG_INFO("Activating MIDI printer...");
    LOG_INFO("  Sample rate: {} Hz", info.sample_rate);
    LOG_INFO("  Frame count range: {} - {}", info.min_frame_size, info.max_frame_size);
    event_count_ = 0;

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
            LOG_INFO("Event #{}: t={} size={} space={}", event_count_, header->time, header->size, header->space_id);

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
    auto port_str = event->port_index >= 0 ? std::to_string(event->port_index) : "wildcard";
    auto chan_str = event->channel >= 0 ? std::to_string(event->channel) : "wildcard";
    auto key_str = event->key >= 0 ? std::to_string(event->key) + " (" + getNoteNameFromKey(event->key) + ")" : "wildcard";
    auto note_id_str = event->note_id >= 0 ? std::to_string(event->note_id) : "unspecified";
    LOG_INFO("  {}: port={} ch={} key={} vel={:.3f} id={}", event_name, port_str, chan_str, key_str, event->velocity, note_id_str);
}

void ExampleMIDIPrinterPlugin::printNoteExpression(const clap_event_note_expression_t* event)
{
    auto port_str = event->port_index >= 0 ? std::to_string(event->port_index) : "wildcard";
    auto chan_str = event->channel >= 0 ? std::to_string(event->channel) : "wildcard";
    auto key_str = event->key >= 0 ? std::to_string(event->key) + " (" + getNoteNameFromKey(event->key) + ")" : "wildcard";
    auto note_id_str = event->note_id >= 0 ? std::to_string(event->note_id) : "wildcard";

    std::string interpretation;
    switch (event->expression_id)
    {
    case CLAP_NOTE_EXPRESSION_VOLUME:
        interpretation = std::format(" [{:.1f} dB]", 20.0 * std::log10(event->value));
        break;
    case CLAP_NOTE_EXPRESSION_PAN:
        interpretation = std::format(" [{:.0f}% {}]",
                                      std::abs(event->value - 0.5) * 200,
                                      event->value < 0.5 ? "Left" : (event->value > 0.5 ? "Right" : "Center"));
        break;
    case CLAP_NOTE_EXPRESSION_TUNING:
        interpretation = std::format(" [{:.2f} cents]", event->value * 100);
        break;
    }

    LOG_INFO("  NOTE_EXPRESSION: {} port={} ch={} key={} id={} val={:.6f}{}",
             getExpressionName(event->expression_id), port_str, chan_str, key_str, note_id_str, event->value, interpretation);
}

void ExampleMIDIPrinterPlugin::printMidiEvent(const clap_event_midi_t* event)
{
    uint8_t status = event->data[0] & 0xF0;
    uint8_t channel = event->data[0] & 0x0F;

    std::string details;
    switch (status)
    {
    case 0x80: // Note Off
    case 0x90: // Note On
        details = std::format("key={} ({}) vel={}", event->data[1], getNoteNameFromKey(event->data[1]), event->data[2]);
        if (status == 0x90 && event->data[2] == 0)
            details += " [vel=0 treated as Note Off]";
        break;
    case 0xA0: // Polyphonic Aftertouch
        details = std::format("key={} ({}) pressure={}", event->data[1], getNoteNameFromKey(event->data[1]), event->data[2]);
        break;
    case 0xB0: // Control Change
        details = std::format("cc={} val={}", event->data[1], event->data[2]);
        break;
    case 0xC0: // Program Change
        details = std::format("program={}", event->data[1]);
        break;
    case 0xD0: // Channel Aftertouch
        details = std::format("pressure={}", event->data[1]);
        break;
    case 0xE0: // Pitch Bend
        details = std::format("bend={}", (event->data[2] << 7) | event->data[1]);
        break;
    }

    LOG_INFO("  MIDI: [{:02X} {:02X} {:02X}] port={} ch={} {} {}",
             event->data[0], event->data[1], event->data[2],
             event->port_index, channel + 1, decodeMidiStatus(status), details);
}

void ExampleMIDIPrinterPlugin::printMidi2Event(const clap_event_midi2_t* event)
{
    uint8_t message_type = (event->data[0] >> 28) & 0x0F;
    uint8_t group = (event->data[0] >> 24) & 0x0F;

    std::string msg_type_str;
    switch (message_type)
    {
    case 0x2: msg_type_str = "MIDI1.0 Chan Voice"; break;
    case 0x3: msg_type_str = "64-bit Data"; break;
    case 0x4:
        {
            uint8_t status = (event->data[0] >> 16) & 0xFF;
            uint8_t channel = (event->data[0] >> 8) & 0x0F;
            msg_type_str = std::format("MIDI2.0 Chan Voice ch={} status={:02X}", channel + 1, status);
        }
        break;
    case 0x5: msg_type_str = "128-bit Data"; break;
    default: msg_type_str = std::format("type={:X}", message_type); break;
    }

    LOG_INFO("  MIDI2: [{:08X} {:08X} {:08X} {:08X}] port={} group={} {}",
             event->data[0], event->data[1], event->data[2], event->data[3],
             event->port_index, group, msg_type_str);
}

void ExampleMIDIPrinterPlugin::printMidiSysexEvent(const clap_event_midi_sysex_t* event)
{
    std::string data_str;
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
            ss << "... (+" << (event->size - 16) << " more)";
        data_str = ss.str();
    }

    LOG_INFO("  SYSEX: port={} size={} bytes [{}]", event->port_index, event->size, data_str);
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

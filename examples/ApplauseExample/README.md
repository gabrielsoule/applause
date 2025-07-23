# Applause Example Plugin

This is a simple example plugin demonstrating how to use the Applause framework to build CLAP plugins.

## Features

- Extends `applause::PluginBase`
- Uses `NotePortsExtension` for MIDI input
- Uses `AudioPortsExtension` for stereo audio output  
- Uses `StateExtension` for state management
- Builds as CLAP, VST3, and AU formats

## Building

From this directory:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

The built plugins will be:
- CLAP: `build/ApplauseExample.clap`
- VST3: `build/ApplauseExample.vst3`
- AU: `build/ApplauseExample.component`

## Implementation Details

The plugin demonstrates:

1. **Basic plugin structure**: Shows how to inherit from `PluginBase` and implement required methods
2. **Extension usage**: Registers and uses multiple extensions
3. **MIDI processing**: Receives and logs MIDI note events
4. **Audio processing**: Simple passthrough/silence generation
5. **Plugin lifecycle**: Proper initialization, activation, and destruction

## Files

- `src/ApplauseExample.h` - Plugin class declaration
- `src/ApplauseExample.cpp` - Plugin implementation
- `src/ApplauseExampleEntry.cpp` - CLAP entry point
- `CMakeLists.txt` - Build configuration
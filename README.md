# Applause

A fast, flexible, and free C++20 framework for building CLAP, VST3, and AU audio plugins.

### Built with CLAP, an open audio plugin ABI
Applause is built upon and tightly integrates with CLAP, an open source and modern audio plugin standard/ABI. CLAP presents a notable improvement over the status quo (VST3, AU, AAX, etc). CLAP is free and open source; furthermore, it exposes powerful features such as polyphonic per-note modulation and host-supported preset discovery (among others). Comprehensive, plug-and-play implementations of these extensions are on our roadmap. 
 
### The easiest way is the fastest way is the default way
Applause is built to squeeze every last cycle out of the CPU. All Applause extensions are designed to be cache friendly and blazing fast. Each line of audio thread code is written with this principle in mind.  Applause's UI system is built on top of Matt Tytel's Visage framework (which, in turn, wraps bgfx). It's fully GPU-accelerated, and supports shaders, complex path rendering, effects, and all other sorts of eye candy -- all at 60FPS. You don't have to worry about "doing things the right way" with Applause utilities: the default way _is_ the right way.

###  Everything you need; nothing that you don't 
Applause is fully modular. The underlying CLAP specification orbits a collection of "extensions" that plugins may or may not implement; Applause implements these extensions via a roughly one-to-one mapping and exposes them to the developer through easy-to-use C++ interfaces. As a plugin developer, you can instantiate the extensions you want and ignore the ones you don't. As such, you will _never_ pay a performance penalty for something you don't care about. And, if you're dissatisfied with Applause's implementation of a CLAP extension, you can just implement the extension yourself. Your custom extension will integrate seamlessly with the rest of the Applause ecosystem.

### Best-in-class implementations of high-level plugin infrastructure
(NYI) Applause will support common plugin infrastructure, such as a SIMD and cache friendly voice management system, along with a (SIMD) polyphonic modulation system. Applause will be furnished with all the tools required to build world-class, professional audio plugins whose performance is on par with the best premium synths on the market.

## Roadmap

Applause is currently (very) early in development. Here are some things that exist, and other things that have not yet come to pass.

### Already Implemented
- CLAP, VST3, and AU support
- Note and audio port extensions
- State saving and loading
- A blazing fast and thread safe plugin parameter extension
- Knob and Slider UI widgets that automatically connect to parameters (plug and play; we handle all the cross-thread messaging for you)
- FlexBox style layouts
- A generic plug-and-play parameter UI with sliders and values (much like JUCE's GenericAudioProcessorEditor)
- CMake utilities to spin up plugins quickly and reduce boilerplate glue code

### On the Roadmap
- Polyphonic parameter modulation with SIMD support
- Synth voice management with SIMD support
- Implementation of the CLAP preset management and discovery extension 
- Implementation of the CLAP polyphonic modulation extension (host knows about active voices and can send polyphonic modulation signals to the plugin)
- Advanced UI components: LFOs, envelopes, filter response curves, etc
- DSP utilities; for now, you can use `chowdsp_utils` for all your DSP needs, but be aware that some parts of that codebase are GPL licensed

## Requirements

- C++20 compiler
- CMake 3.19+

## Building

```bash
# Configure
cmake -B build

# Build everything (framework + all examples)
cmake --build build

# Build framework only
cmake --build build --target applause

# Build all examples
cmake --build build --target applause-examples

# Build specific example (all formats)
cmake --build build --target ExampleShowcase_all
cmake --build build --target ExampleManualPluginEntry_all

# Build specific format
cmake --build build --target ExampleShowcase_clap
cmake --build build --target ExampleShowcase_vst3
cmake --build build --target ExampleShowcase_auv2
```
## License

MIT
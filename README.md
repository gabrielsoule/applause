

# Applause

A fast, flexible, and free C++20 framework for building CLAP, VST3, and AU audio plugins.

### Built with CLAP, an open audio plugin ABI
Applause is built upon and tightly integrates with CLAP, an open source and modern audio plugin standard/ABI. CLAP presents a notable improvement over the status quo (VST3, AU, AAX, etc). CLAP is free and open source; furthermore, it exposes powerful features such as polyphonic per-note modulation and host-supported preset discovery (among others). Comprehensive, plug-and-play implementations of these extensions are on our roadmap. 
 
### The easiest way is the fastest way is the default way
Applause is designed to squeeze every last cycle out of the CPU. All Applause extensions are designed to be cache friendly and blazing fast. The UI system is built on top of Matt Tytel's Visage framework, which in turn wraps bgfx: it's GPU accelerated, runs at 60 FPS without breaking a sweat, and supports modern widgets like path rendering, shaders, effects, et cetera. 

###  Everything you need; nothing that you don't 
Applause is fully modular. The underlying CLAP specification orbits a collection of "extensions" that plugins may or may not implement; Applause implements these extensions via a roughly one-to-one mapping and exposes them to the developer through easy-to-use C++ interfaces. As a plugin developer, you can instantiate the extensions you want and ignore the ones you don't. As such, you will _never_ pay a performance penalty for something you don't care about. If you're dissatisfied with Applause's implementation of a CLAP extension, you can implement the extension yourself. Your custom extension will integrate seamlessly with the rest of the Applause ecosystem.

### Best-in-class implementations of high-level plugin infrastructure
(NYI) Applause will support common plugin infrastructure, such as a SIMD and cache friendly voice management system, along with a (SIMD) polyphonic parameter modulation system. Hopefully you'll never have to write your own modulation matrix system again. 

## Roadmap

Applause is currently (very) early in development. Here are some things that exist, and other things that have not yet come to pass.

### Already Implemented
- CLAP, VST3, and AU support
- Note and audio port extensions
- State saving and loading
- Fast and thread safe hosted parameter management
- Knob and Slider UI widgets that automatically connect to parameters (plug and play; we handle all the cross-thread messaging for you)
- FlexBox style layouts
- A generic plug-and-play parameter UI with sliders and values (much like JUCE's GenericAudioProcessorEditor)
- CMake utilities to spin up plugins quickly and reduce boilerplate glue code
- A simple Synthesizer class 

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
There are several ready-to-deploy Applause examples in the 'examples' directory. They can be built with the `applause-examples` CMake target.

## License

MIT
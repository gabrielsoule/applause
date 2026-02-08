

# Applause

A fast, flexible, and free C++20 framework for building CLAP, VST3, and AU audio plugins.

Applause is still a work in progress, but you _can_ make fully functional plugins with what exists so far. Check out the examples subdirectory.

There's lots of work to do. Pull requests, comments, and suggestions are all welcome. 

### Built with CLAP, an open audio plugin ABI
Applause is built upon and tightly integrates with CLAP, an open source and modern audio plugin standard/ABI. CLAP presents a notable improvement over the status quo (VST3, AU, AAX, etc). CLAP is free and open source; furthermore, it exposes powerful features such as polyphonic per-note modulation and host-supported preset discovery (among others).
 
### The easiest way is the fastest way is the default way
Applause is designed to be fast, with cache-friendly data structures and carefully designed DSP procedures. It should be easy to do things the "right" way. Don't overthink stuff.

###  Everything you need; nothing that you don't 
Applause is fully modular. The underlying CLAP specification is composed of C "extensions" that plugins may or may not implement: think parameters, state, presets, et cetera. Applause implements these extensions and exposes them to the plugin developer through easy-to-use C++ interfaces. As a plugin developer, you can instantiate the extensions you want and ignore the ones you don't. As such, you will never pay a performance penalty for something you don't care about. If you're dissatisfied with Applause's implementation of a CLAP extension, you can implement the extension yourself. Your custom extension will integrate seamlessly with the rest of the Applause ecosystem. Extensions can be written in C++ using a helpful wrapper, or in good old C (if you are into that kind of thing).

### GPU-accelerated UI
Applause's UI layer is built on top of Matt Tytel's visage library, which in turn inherits from the Vital synthesizer. The UI is GPU-accelerated and supports 60fps animations, shaders, gradients, et cetera. Applause supplies a collection of useful plugin widgets such as knobs and sliders, which can directly connect to host parameters if desired. It's all plug-and-play; we handle the cross-thread shenanigans for you.

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

### Implemented, but in active development
- A polyphonic modulation system (the DSP side works; but several UI components such as a mod matrix list and knob visualizations are WIP)

### On the Roadmap
- Implementation of the CLAP preset management and discovery extension 
- Implementation of the CLAP polyphonic modulation extension (host knows about active voices and can send polyphonic modulation signals to the plugin)
- UI components: LFOs, envelopes, filter response curves, etc
- DSP utilities; for now, you can use `chowdsp_utils` for all your DSP needs, but be aware that some parts of that codebase are GPL licensed

## Requirements

Applause uses C++20 features. I may consider downgrading to C++17 if this proves to be exceedingly unpopular, though I do enjoy the C++20 QoL features.

## Building
There are several ready-to-deploy Applause examples in the 'examples' directory. They can be built with the `applause-examples` CMake target. 

## License

MIT. Some of the chowdsp_utils modules are pubished under less permissive licenses, so if you use chowdsp_utils for DSP purposes, please be aware of any licensing implications.

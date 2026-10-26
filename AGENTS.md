# Repository Guidelines

## Project Structure & Architecture

Applause is a C++20 framework for CLAP, VST3, and AUv2 plugins. Its static-library target is `Applause::Applause`.

- `applause/core/`: plugin lifecycle, `ProcessContext`, and scalar/SIMD `ModMatrix<Signal>`.
- `applause/extensions/`: parameters, audio/note ports, state, GUI, and thread-pool extensions.
- `applause/dsp/`: voices, buffers, note expressions, filters, and MSEG modulation.
- `applause/graphics/`, `applause/windowing/`: GPU rendering (bgfx) and native windows, derived from Visage.
- `applause/ui/`: frames, events, layout, editors, and components; `applause/util/`: memory, messaging, sample types, UI utilities, and inspector.
- `examples/`: plugin projects, including `ExampleSineWaveSynth`, `ExampleShowcase`, and `ExampleSimdPolySynth`.
- `tests/`: mirrors source modules; `resources/fonts/`: embedded fonts.

## Build, Test, and Development Commands

Use CMake 3.21+ and a C++20 compiler. CPM fetches dependencies. From the repository root:

```bash
# Configure with tests; build library and tests
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug -DAPPLAUSE_BUILD_TESTS=ON
cmake --build cmake-build-debug --target applause applause_tests

# Build all examples or one plugin
cmake --build cmake-build-debug --target applause-examples
cmake --build cmake-build-debug --target ExampleSineWaveSynth_all

# Run all tests or filter by Catch2 tag
ctest --test-dir cmake-build-debug --output-on-failure
./cmake-build-debug/tests/applause_tests "[modmatrix]"
```

`APPLAUSE_BUILD_EXAMPLES` defaults on for top-level builds. Tests default off. Reconfigure after adding library sources. Load examples in a compatible plugin host.

## Coding Style & Naming Conventions

Follow `.clang-format`: Google-based C++, four-space indentation, attached braces, and 120-column limit. Use PascalCase types/files and camelCase methods; match nearby member naming. Avoid redundant comments and one-use abstractions.

## Plugin and UI Development

Use `add_applause_plugin()` in `cmake/ApplausePlugin.cmake`; select `PLUGIN_FORMATS` (default: CLAP). Match features to registered ports: instruments need note input/audio output; effects need audio input/output.

Register extension instances with `registerExtension(instance)`; retrieve with `getExtension<T>()`. Override `ProcessStatus process(ProcessContext&) noexcept`. Prepare allocations and parameter handles outside audio processing; keep `ParamInfo` on the main thread. Modulation remains under development.

Dynamic parameters come from a pool (`activateDynamicParam()`/`deactivateDynamicParam()`), stay internal, and reach the DAW only through a fixed pool of automation slots (`ParamInfo::setAutomatable()`). Follow the ordering rules in the `ParamsExtension` class comment. `ModMatrix` hands the audio thread a rebuilt read-only `ModProgram` after every graph edit, and follows dynamic params through `activateDestination()`/`deactivateDestination()`.

The UI layer is derived from Visage and lives in-tree in `namespace applause`; read it before writing UI code. Theme colors and sizes are tokens (`applause/ui/Theme.h`): override them per frame with `set()`, and read them with `color(frame, token)` and `value(frame, token)`. A see-through tint is its base color with an opacity `ValueToken`, applied in draw code. Buttons are one `Button` class (text, icon, or both) with `setCheckable()`, `setRole()`, `onClick()`, and `onToggle(bool)`.

TODO: only macOS has been compiled since Visage was merged in; add Windows and Linux CI.

## Testing Guidelines

Use Catch2 v3 in `tests/<module>/*Test.cpp`, with descriptive cases and tags. New test `.cpp` files are discovered automatically. Add relevant regression coverage; no numeric threshold is configured. Run proportional checks and report commands, results, and failures.

## Commit & Pull Request Guidelines

History favors imperative subjects (“Add”, “Fix”, “Refactor”) without strict prefixes. Explain behavior changes, link relevant issues, report validation, and include screenshots for UI changes. Identify breaking API changes explicitly.

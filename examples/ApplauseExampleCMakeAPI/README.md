# Applause Example Plugin with CMake API

This example demonstrates how to use the simplified Applause CMake API to build CLAP plugins with minimal boilerplate.

## Key Features

The `applause_plugin()` CMake function automatically generates all plugin entry point code, eliminating the need for manual boilerplate. You only need to:

1. Write your plugin class inheriting from `applause::PluginBase`
2. Call `applause_plugin()` in CMakeLists.txt with your plugin details
3. Build - the entry point code is generated automatically!

## Benefits

- **No manual entry point code** - The plugin factory, descriptor, and CLAP entry are generated
- **Multi-format support** - Easily build CLAP, VST3, and AU from the same source
- **Cleaner codebase** - Focus on your plugin logic, not boilerplate
- **Type-safe** - All generated code uses your specified class names

## Usage

See `CMakeLists.txt` for the complete example of using `applause_plugin()`.

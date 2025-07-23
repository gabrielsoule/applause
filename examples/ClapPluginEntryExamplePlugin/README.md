Applause is furnished with CMake functions that automate the constructon of the CLAP plugin entry .cpp file. This is a necessary chunk of plumbing that all CLAP plugins require. 
 
This whole affair is somewhat cumbersome and unintuitive, especially for those unfamilar with C ABI shenanigans. As such, Applause can automatically generate this code during build.

However, some plugins may wish to handle the low-level CLAP plugin entry themselves. Applause will never force the developer to use its own QOL scripts and APIs. To that end, this example shows how to use the CLAP plugin entry interface the  "old fashioned way".

Instead of invoking Applause's CMake build helpers, we directly invoke the clap-wrapper build functions to generate CLAP, VST3, and AU builds of our plugin. 
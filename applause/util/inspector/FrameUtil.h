#pragma once

#ifndef NDEBUG

#include <applause/ui/ApplauseUI.h>

#include <string>

namespace applause::inspector {

// Demangle a typeid name. Libc++/libstdc++ via abi::__cxa_demangle; MSVC
// strips the "class "/"struct " prefix that typeid returns.
std::string demangle(const char* mangled);

// Best-effort human-readable label for a Frame: name() if set, else demangled
// dynamic type via typeid(frame).name().
std::string frameTypeName(const applause::Frame& frame);

// Visage's Frame::frameAtPoint with a subtree-exclusion parameter. Used by
// pick mode so the inspector's own overlay/panel don't capture the hit.
// `point` is in `root`'s parent coordinate space (window coords when called
// with the editor as root).
applause::Frame* frameAtPointExcluding(applause::Frame* root,
                                       applause::Point point,
                                       applause::Frame* exclude);

// Count this frame and all descendants (cheap; used to detect tree changes).
int recursiveFrameCount(const applause::Frame* frame);

}  // namespace applause::inspector

#endif  // NDEBUG

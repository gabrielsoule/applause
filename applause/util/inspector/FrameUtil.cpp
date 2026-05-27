#include "FrameUtil.h"

#ifndef NDEBUG

#include <cstdlib>
#include <memory>
#include <typeinfo>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

namespace applause::inspector {

std::string demangle(const char* mangled) {
    if (!mangled) return {};
#if defined(__GNUC__) || defined(__clang__)
    int status = 0;
    std::unique_ptr<char, void (*)(void*)> result(
        abi::__cxa_demangle(mangled, nullptr, nullptr, &status), std::free);
    if (status == 0 && result) return result.get();
    return mangled;
#else
    std::string s = mangled;
    if (s.rfind("class ", 0) == 0) s.erase(0, 6);
    else if (s.rfind("struct ", 0) == 0) s.erase(0, 7);
    return s;
#endif
}

std::string frameTypeName(const applause::Frame& frame) {
    const auto& n = frame.name();
    if (!n.empty()) return n;
    return demangle(typeid(frame).name());
}

applause::Frame* frameAtPointExcluding(applause::Frame* root,
                                       applause::Point point,
                                       applause::Frame* exclude) {
    if (!root || root == exclude) return nullptr;

    const auto& children = root->children();
    // Walk on_top siblings first (drawn last → on top), reverse iter so
    // last-added/topmost wins. Matches visage::Frame::frameAtPoint order.
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        auto* child = *it;
        if (child == exclude || !child->isOnTop()) continue;
        if (!child->isVisible() || !child->containsPoint(point)) continue;
        if (auto* hit = frameAtPointExcluding(child, point - child->topLeft(), exclude))
            return hit;
    }
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        auto* child = *it;
        if (child == exclude || child->isOnTop()) continue;
        if (!child->isVisible() || !child->containsPoint(point)) continue;
        if (auto* hit = frameAtPointExcluding(child, point - child->topLeft(), exclude))
            return hit;
    }
    return root->ignoresMouseEvents() ? nullptr : root;
}

int recursiveFrameCount(const applause::Frame* frame) {
    if (!frame) return 0;
    int n = 1;
    for (const auto* c : frame->children())
        n += recursiveFrameCount(c);
    return n;
}

}  // namespace applause::inspector

#endif  // NDEBUG

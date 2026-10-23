#pragma once

#include <applause/graphics/Color.h>

#include <concepts>
#include <variant>
#include <vector>

namespace applause {
class Frame;

// A named theme value with a default. The default is a fixed value, or another token's value as seen from the same
// frame, so setting a token on a frame also changes the tokens that follow it there. A token is identified by its
// address; its name is for display.
template <typename T>
class Token {
public:
    Token(const char* name, T value) : name_(name), value_(value) { all().push_back(this); }
    Token(const char* name, const Token& base) : name_(name), base_(&base) { all().push_back(this); }
    // A color can follow another at a fraction of its alpha.
    Token(const char* name, const Token& base, float alpha)
        requires std::same_as<T, Color>
        : name_(name), base_(&base), alpha_(alpha) {
        all().push_back(this);
    }

    Token(const Token&) = delete;
    Token& operator=(const Token&) = delete;

    const char* name() const { return name_; }
    // The token this one follows when nothing sets it, or null when its default is a fixed value.
    const Token* base() const { return base_; }
    // The fraction of its base's alpha a color keeps.
    float alpha() const { return alpha_; }

    // Every token of this type.
    static std::vector<const Token*>& all() {
        static std::vector<const Token*> tokens;
        return tokens;
    }

private:
    friend Color color(const Frame& frame, const Token<Color>& token);
    friend float value(const Frame& frame, const Token<float>& token);

    T resolve(const Frame& frame) const;

    const char* name_;
    T value_{};
    const Token* base_ = nullptr;
    float alpha_ = 1.0f;
};

using ColorToken = Token<Color>;
using ValueToken = Token<float>;

// What a frame sets a token to: a value, or another token to follow.
template <typename T>
using Setting = std::variant<T, const Token<T>*>;

// A token's value at a frame. The nearest frame at or above it that sets the token decides; when none does, the
// token's default applies. Tokens followed either way are read from the same frame.
Color color(const Frame& frame, const ColorToken& token);
float value(const Frame& frame, const ValueToken& token);

namespace theme {
// The highlight color of toggles, glows, and selections.
inline const ColorToken kAccent{"accent", 0xff9966ff};
}  // namespace theme

}  // namespace applause

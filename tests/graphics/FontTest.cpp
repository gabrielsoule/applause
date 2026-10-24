/* Copyright Vital Audio, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <applause/graphics/Canvas.h>
#include <applause/graphics/Font.h>
#include "LatoRegular.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace applause;
using namespace Catch;

TEST_CASE("Font embedded test", "[graphics]") {
    Font font(16, fonts::Lato_Regular_ttf_data, sizeof(fonts::Lato_Regular_ttf_data));
    Canvas canvas;
    canvas.setWindowless(100, 50);
    canvas.text("Test", font, Font::Justification::kCenter, 0, 0, 100, 50);
    canvas.submit();
    canvas.takeScreenshot();
    canvas.submit();
}

TEST_CASE("Letter spacing adds to each glyph's advance", "[graphics]") {
    Font font(16, fonts::Lato_Regular_ttf_data, sizeof(fonts::Lato_Regular_ttf_data), 2.0f);
    Font spaced = font.withLetterSpacing(3.0f);
    const std::u32string text = U"Test";

    REQUIRE(spaced.letterSpacing() == 3.0f);
    REQUIRE(spaced.stringWidth(text) == Approx(font.stringWidth(text) + 3.0f * text.size()));
    REQUIRE(spaced.withSize(20).letterSpacing() == 3.0f);
    REQUIRE(spaced.withDpiScale(1.0f).letterSpacing() == 3.0f);

    const float width = font.stringWidth(text);
    REQUIRE(font.widthOverflowIndex(text.c_str(), text.size(), width) == 4);
    REQUIRE(spaced.widthOverflowIndex(text.c_str(), text.size(), width) < 4);
}

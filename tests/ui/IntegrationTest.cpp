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

#include <applause/ui/ApplicationEditor.h>
#include <applause/ui/ApplicationWindow.h>
#include <applause/ui/ClientWindowDecoration.h>
#include <applause/ui/WindowEventHandler.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <applause/ui/Events.h>
#include <applause/ui/Frame.h>
#include <applause/ui/Layout.h>
#include <applause/ui/PopupMenu.h>
#include <applause/ui/ScrollBar.h>
#include <applause/ui/SvgFrame.h>
#include <applause/ui/UndoHistory.h>

using namespace applause;
using namespace Catch;

TEST_CASE("Screenshot solid color", "[integration]") {
  Canvas canvas;
  canvas.setWindowless(10, 5);
  canvas.setColor(0xffddaa88);
  canvas.fill(0, 0, canvas.width(), canvas.height());
  canvas.submit();

  canvas.takeScreenshot();
  const Screenshot& screenshot = canvas.screenshot();

  REQUIRE(screenshot.width() == 10);
  REQUIRE(screenshot.height() == 5);
  const uint8_t* data = screenshot.data();
  int i = 0;
  for (int y = 0; y < 5; ++y) {
    for (int x = 0; x < 10; ++x) {
      REQUIRE(data[i++] == 0xdd);
      REQUIRE(data[i++] == 0xaa);
      REQUIRE(data[i++] == 0x88);
      REQUIRE(data[i++] == 0xff);
    }
  }
}

TEST_CASE("Screenshot vertical gradient", "[integration]") {
  Color source = 0xff345678;
  Color destination = 0xff88aacc;
  ApplicationEditor editor;
  editor.onDraw() = [&](Canvas& canvas) {
    canvas.setColor(Brush::vertical(source, destination));
    canvas.fill(0, 0, editor.width(), editor.height());
  };

  editor.setWindowless(10, 5);
  Screenshot screenshot = editor.takeScreenshot();
  REQUIRE(screenshot.width() == 10);
  REQUIRE(screenshot.height() == 5);
  uint8_t* data = screenshot.data();
  int i = 0;
  for (int y = 0; y < 5; ++y) {
    float t = y / 4.0f;
    Color sample = source.interpolateWith(destination, t);

    for (int x = 0; x < 10; ++x) {
      REQUIRE(data[i++] == sample.hexRed());
      REQUIRE(data[i++] == sample.hexGreen());
      REQUIRE(data[i++] == sample.hexBlue());
      REQUIRE(data[i++] == 0xff);
    }
  }
}

TEST_CASE("Screenshot horizontal gradient", "[integration]") {
  Color source = 0xff123456;
  Color destination = 0xff88aacc;
  ApplicationEditor editor;
  editor.onDraw() = [&](Canvas& canvas) {
    canvas.setColor(Brush::horizontal(source, destination));
    canvas.fill(0, 0, editor.width(), editor.height());
  };

  editor.setWindowless(10, 5);
  Screenshot screenshot = editor.takeScreenshot();
  REQUIRE(screenshot.width() == 10);
  REQUIRE(screenshot.height() == 5);
  uint8_t* data = screenshot.data();
  for (int x = 0; x < 10; ++x) {
    float t = x / 9.0f;
    Color sample = source.interpolateWith(destination, t);

    for (int y = 0; y < 5; ++y) {
      int index = (y * 10 + x) * 4;
      REQUIRE(std::abs(data[index] - (int)sample.hexRed()) <= 1);
      REQUIRE(std::abs(data[index + 1] - (int)sample.hexGreen()) <= 1);
      REQUIRE(std::abs(data[index + 2] - (int)sample.hexBlue()) <= 1);
      REQUIRE(std::abs(data[index + 3] - 0xff) <= 1);
    }
  }
}

TEST_CASE("Grandchild overlapping order", "[integration]") {
  ApplicationEditor editor;
  applause::Frame trigger;

  applause::Frame container;
  applause::Frame wrapper;
  applause::Frame bottom;
  applause::Frame top;

  editor.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff333333);
    canvas.fill();
  };

  trigger.setBounds(20, 20, 100, 100);
  trigger.onDraw() = [](applause::Canvas& c) {
    c.setColor(0xFFFF0000);
    c.fill();
  };

  container.setBounds(150, 20, 100, 100);
  wrapper.setBounds(0, 0, 100, 100);
  bottom.setBounds(0, 0, 100, 100);
  top.setBounds(0, 0, 100, 100);

  container.addChild(&wrapper);
  wrapper.addChild(&bottom);
  container.addChild(&top);

  bottom.onDraw() = [](applause::Canvas& c) {
    c.setColor(0xFF00FF00);
    c.roundedRectangle(0, 0, 100, 100, 5);
  };

  top.onDraw() = [](applause::Canvas& c) {
    c.setColor(0xFFFFFF00);
    c.roundedRectangle(0, 0, 100, 100, 5);
  };

  editor.addChild(&trigger);
  editor.addChild(&container);
  editor.setWindowless(300, 150);
  Screenshot screenshot = editor.takeScreenshot();
  uint8_t* data = screenshot.data();

  int y = 30;
  int x = 160;
  int index = (y * 300 + x) * 4;
  REQUIRE(data[index] == 0xff);
  REQUIRE(data[index + 1] == 0xff);
  REQUIRE(data[index + 2] == 0x00);
  REQUIRE(data[index + 3] == 0xff);
}

TEST_CASE("Add/remove child multiple times", "[integration]") {
  ApplicationEditor app;
  applause::Frame child;

  app.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff000000);
    canvas.fill();
  };

  child.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xffffffff);
    canvas.fill();
  };

  child.setBounds(0, 0, 50, 50);
  app.addChild(child);
  app.setWindowless(50, 50);

  for (int i = 0; i < 10; ++i) {
    app.drawWindow();
    Screenshot screenshot = app.takeScreenshot();
    uint8_t* data = screenshot.data();

    child.redraw();
    int index = 100;
    REQUIRE(data[index + 3] == 0xff);
    if (i % 2) {
      REQUIRE(data[index] == 0x00);
      REQUIRE(data[index + 1] == 0x00);
      REQUIRE(data[index + 2] == 0x00);
      app.addChild(child);
    }
    else {
      REQUIRE(data[index] == 0xff);
      REQUIRE(data[index + 1] == 0xff);
      REQUIRE(data[index + 2] == 0xff);
      app.removeChild(child);
    }
  }
}

TEST_CASE("Overlap test - grouping with overlaps bug", "[integration]") {
  applause::ApplicationWindow app;

  applause::Frame trigger;
  applause::Frame container;
  applause::Frame wrapper;
  applause::Frame bottom;
  applause::Frame top;

  app.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff333333);
    canvas.fill(0, 0, app.width(), app.height());
  };

  trigger.setBounds(0, 0, 25, 25);
  trigger.onDraw() = [](applause::Canvas& c) {
    c.setColor(0xFFFF0000);
    c.fill();
  };

  container.setBounds(30, 0, 20, 25);
  wrapper.setBounds(0, 0, 20, 25);
  bottom.setBounds(0, 0, 20, 25);
  top.setBounds(0, 0, 20, 25);

  container.addChild(&wrapper);
  wrapper.addChild(&bottom);
  container.addChild(&top);

  bottom.onDraw() = [](applause::Canvas& c) {
    c.setColor(0xFF00FF00);
    c.fill();
  };

  top.onDraw() = [&top](applause::Canvas& c) {
    c.setColor(0xFFFF00FF);
    c.fill();
  };

  app.addChild(&trigger);
  app.addChild(&container);
  app.setWindowless(50, 50);

  Screenshot screenshot = app.takeScreenshot();
  uint8_t* data = screenshot.data();
  int index = (5 * 50 + 35) * 4;
  REQUIRE(data[index] == 0xff);
  REQUIRE(data[index + 1] == 0x00);
  REQUIRE(data[index + 2] == 0xff);
  REQUIRE(data[index + 3] == 0xff);
}

TEST_CASE("Overlap test - several overlapping siblings", "[integration]") {
  applause::ApplicationWindow app;

  applause::Frame one;
  applause::Frame two;
  applause::Frame three;

  one.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xffcf6944);
    canvas.fill();
  };

  two.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff8d9f8c);
    canvas.fill();
  };

  three.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xffffffff);
    canvas.fill();
  };

  one.setBounds(0, 0, 50, 50);
  two.setBounds(0, 0, 50, 50);
  three.setBounds(0, 0, 50, 50);

  app.addChild(one);
  app.addChild(two);
  app.addChild(three);

  app.setWindowless(50, 50);

  Screenshot screenshot = app.takeScreenshot();
  uint8_t* data = screenshot.data();
  int index = 100;
  REQUIRE(data[index] == 0xff);
  REQUIRE(data[index + 1] == 0xff);
  REQUIRE(data[index + 2] == 0xff);
  REQUIRE(data[index + 3] == 0xff);
}

TEST_CASE("Overlap test - several overlapping children", "[integration]") {
  applause::ApplicationWindow app;

  applause::Frame parent;
  applause::Frame one;
  applause::Frame two;
  applause::Frame three;

  one.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xffcf6944);
    canvas.fill();
  };

  two.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff8d9f8c);
    canvas.fill();
  };

  three.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xffffffff);
    canvas.fill();
  };

  parent.setBounds(0, 0, 50, 50);

  one.setBounds(0, 0, 50, 50);
  two.setBounds(0, 0, 50, 50);
  three.setBounds(0, 0, 50, 50);

  parent.addChild(one);
  parent.addChild(two);
  parent.addChild(three);

  app.addChild(parent);
  app.setWindowless(50, 50);

  Screenshot screenshot = app.takeScreenshot();
  uint8_t* data = screenshot.data();
  int index = 100;
  REQUIRE(data[index] == 0xff);
  REQUIRE(data[index + 1] == 0xff);
  REQUIRE(data[index + 2] == 0xff);
  REQUIRE(data[index + 3] == 0xff);
}

TEST_CASE("Overlap test - niece/nephew overlap order", "[integration]") {
  applause::ApplicationWindow app;

  applause::Frame parent;
  applause::Frame child;
  applause::Frame overlay;
  applause::Frame modal;

  parent.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff000000);
    canvas.fill();
  };

  child.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff8d9f8c);
    canvas.fill();
  };

  overlay.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff222222);
    canvas.fill();
  };

  modal.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xffffffff);
    canvas.fill();
  };

  parent.setBounds(0, 0, 50, 50);
  child.setBounds(0, 0, 50, 50);
  overlay.setBounds(0, 0, 50, 50);
  modal.setBounds(0, 0, 50, 50);

  parent.addChild(&child);
  parent.addChild(&overlay);

  app.addChild(&parent);
  app.addChild(&modal);

  app.setWindowless(50, 50);

  Screenshot screenshot = app.takeScreenshot();
  uint8_t* data = screenshot.data();
  int index = 100;
  REQUIRE(data[index] == 0xff);
  REQUIRE(data[index + 1] == 0xff);
  REQUIRE(data[index + 2] == 0xff);
  REQUIRE(data[index + 3] == 0xff);
}

TEST_CASE("Overlap test - niece/nephew overlap order 2", "[integration]") {
  applause::ApplicationWindow app;

  applause::Frame parent;
  applause::Frame child;
  applause::Frame overlay;
  applause::Frame modal;

  child.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff8d9f8c);
    canvas.fill();
  };

  overlay.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff222222);
    canvas.fill();
  };

  modal.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xffffffff);
    canvas.fill();
  };

  parent.setBounds(0, 0, 50, 50);
  child.setBounds(0, 0, 50, 50);
  overlay.setBounds(0, 0, 50, 50);
  modal.setBounds(0, 0, 50, 50);

  parent.addChild(&child);
  parent.addChild(&overlay);

  app.addChild(&parent);
  app.addChild(&modal);

  app.setWindowless(50, 50);

  Screenshot screenshot = app.takeScreenshot();
  uint8_t* data = screenshot.data();
  int index = 100;
  REQUIRE(data[index] == 0xff);
  REQUIRE(data[index + 1] == 0xff);
  REQUIRE(data[index + 2] == 0xff);
  REQUIRE(data[index + 3] == 0xff);
}

TEST_CASE("Overlap test - non transitive overlap", "[integration]") {
  applause::ApplicationWindow app;

  applause::Frame one;
  applause::Frame two;
  applause::Frame three;

  one.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xffff0000);
    canvas.fill();
  };

  two.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff00ff00);
    canvas.fill();
  };

  three.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff0000ff);
    canvas.fill();
  };

  one.setBounds(0, 0, 20, 50);
  two.setBounds(10, 0, 30, 50);
  three.setBounds(30, 0, 20, 50);

  app.addChild(one);
  app.addChild(two);
  app.addChild(three);

  app.setWindowless(50, 50);

  Screenshot screenshot = app.takeScreenshot();
  uint8_t* data = screenshot.data();
  int y = 30;
  int index1 = (y * 50 + 5) * 4;
  REQUIRE(data[index1] == 0xff);
  REQUIRE(data[index1 + 1] == 0x00);
  REQUIRE(data[index1 + 2] == 0x00);
  REQUIRE(data[index1 + 3] == 0xff);

  int index2 = (y * 50 + 15) * 4;
  REQUIRE(data[index2] == 0x00);
  REQUIRE(data[index2 + 1] == 0xff);
  REQUIRE(data[index2 + 2] == 0x00);
  REQUIRE(data[index2 + 3] == 0xff);

  int index3 = (y * 50 + 35) * 4;
  REQUIRE(data[index3] == 0x00);
  REQUIRE(data[index3 + 1] == 0x00);
  REQUIRE(data[index3 + 2] == 0xff);
  REQUIRE(data[index3 + 3] == 0xff);
}

TEST_CASE("Overlap test - non transitive overlap after overlap", "[integration]") {
  applause::ApplicationWindow app;

  applause::Frame zero;
  applause::Frame one;
  applause::Frame two;
  applause::Frame three;

  zero.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff000000);
    canvas.fill();
  };

  one.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xffff0000);
    canvas.fill();
  };

  two.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff00ff00);
    canvas.fill();
  };

  three.onDraw() = [&](applause::Canvas& canvas) {
    canvas.setColor(0xff0000ff);
    canvas.fill();
  };

  zero.setBounds(0, 0, 50, 50);
  one.setBounds(0, 0, 20, 50);
  two.setBounds(10, 0, 30, 50);
  three.setBounds(30, 0, 20, 50);

  app.addChild(zero);
  app.addChild(one);
  app.addChild(two);
  app.addChild(three);

  app.setWindowless(50, 50);

  Screenshot screenshot = app.takeScreenshot();
  uint8_t* data = screenshot.data();
  int y = 30;
  int index1 = (y * 50 + 5) * 4;
  REQUIRE(data[index1] == 0xff);
  REQUIRE(data[index1 + 1] == 0x00);
  REQUIRE(data[index1 + 2] == 0x00);
  REQUIRE(data[index1 + 3] == 0xff);

  int index2 = (y * 50 + 15) * 4;
  REQUIRE(data[index2] == 0x00);
  REQUIRE(data[index2 + 1] == 0xff);
  REQUIRE(data[index2 + 2] == 0x00);
  REQUIRE(data[index2 + 3] == 0xff);

  int index3 = (y * 50 + 35) * 4;
  REQUIRE(data[index3] == 0x00);
  REQUIRE(data[index3 + 1] == 0x00);
  REQUIRE(data[index3 + 2] == 0xff);
  REQUIRE(data[index3 + 3] == 0xff);
}
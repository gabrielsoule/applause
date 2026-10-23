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

#include <applause/graphics/Shapes.h>

#include <embedded/shaders.h>
#include <applause/graphics/Layer.h>
#include <applause/graphics/Region.h>

#define APPLAUSE_SET_PROGRAM(shape, vertex, fragment) \
  const EmbeddedFile& shape::vertexShader() {       \
    return vertex;                                  \
  }                                                 \
  const EmbeddedFile& shape::fragmentShader() {     \
    return fragment;                                \
  }

namespace applause {
  APPLAUSE_SET_PROGRAM(Fill, shaders::vs_color, shaders::fs_color)
  APPLAUSE_SET_PROGRAM(Rectangle, shaders::vs_shape, shaders::fs_rectangle)
  APPLAUSE_SET_PROGRAM(RoundedRectangle, shaders::vs_shape, shaders::fs_rounded_rectangle)
  APPLAUSE_SET_PROGRAM(Circle, shaders::vs_shape, shaders::fs_circle)
  APPLAUSE_SET_PROGRAM(Squircle, shaders::vs_shape, shaders::fs_squircle)
  APPLAUSE_SET_PROGRAM(FlatArc, shaders::vs_arc, shaders::fs_flat_arc)
  APPLAUSE_SET_PROGRAM(RoundedArc, shaders::vs_arc, shaders::fs_rounded_arc)
  APPLAUSE_SET_PROGRAM(FlatSegment, shaders::vs_complex_shape, shaders::fs_flat_segment)
  APPLAUSE_SET_PROGRAM(RoundedSegment, shaders::vs_complex_shape, shaders::fs_rounded_segment)
  APPLAUSE_SET_PROGRAM(Triangle, shaders::vs_complex_shape, shaders::fs_triangle)
  APPLAUSE_SET_PROGRAM(QuadraticBezier, shaders::vs_complex_shape, shaders::fs_quadratic_bezier)
  APPLAUSE_SET_PROGRAM(Diamond, shaders::vs_shape, shaders::fs_diamond)
  APPLAUSE_SET_PROGRAM(ImageWrapper, shaders::vs_tinted_texture, shaders::fs_tinted_texture)
  APPLAUSE_SET_PROGRAM(PathFillWrapper, shaders::vs_sample_path, shaders::fs_sample_path)
  APPLAUSE_SET_PROGRAM(GraphLineWrapper, shaders::vs_shape, shaders::fs_graph_line)
  APPLAUSE_SET_PROGRAM(GraphFillWrapper, shaders::vs_shape, shaders::fs_graph_fill)
  APPLAUSE_SET_PROGRAM(HeatMapWrapper, shaders::vs_shape, shaders::fs_heat_map)
  APPLAUSE_SET_PROGRAM(SampleRegion, shaders::vs_post_effect, shaders::fs_post_effect)

  SampleRegion::SampleRegion(const ClampBounds& clamp, const PackedBrush* brush, float x, float y,
                             float width, float height, const Region* region, PostEffect* post_effect) :
      Shape(region->layer(), clamp, brush, x, y, width, height), region(region),
      post_effect(post_effect) {
    if (post_effect)
      batch_id = post_effect;
  }

  void SampleRegion::setVertexData(Vertex* vertices) const {
    region->layer()->setTexturePositionsForRegion(region, vertices);
  }
}

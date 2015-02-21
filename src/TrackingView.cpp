// OpenGL viewer for a TrackingBuffer (c) 2015 Micah Elizabeth Scott
// MIT license

#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "TrackingView.h"
#include <algorithm>

using namespace std;
using namespace ci;


void TrackingView::setup()
{
    mFrameTextures.clear();
}

void TrackingView::draw(TrackingBuffer &buffer)
{
    // Draw all previous frames, in temporal order
    uint32_t frame_counter = buffer.data()->header.frame_counter;
    uint32_t first_frame = max<int64_t>(0, int64_t(frame_counter) - (buffer.kNumFrames - 1));
    for (uint32_t i = first_frame; i < frame_counter; i++) {
        drawFrame(buffer, i, 0.1f);
    }
}

void TrackingView::drawFrame(TrackingBuffer &buffer, unsigned index, float alpha)
{
    uint8_t ring_index = index & (buffer.kNumFrames - 1);
    auto& frame = buffer.data()->frames[ring_index];
    
    if (mFrameTextures.size() <= ring_index) {
        mFrameTextures.resize(ring_index + 1);
    }
    auto& tex = mFrameTextures[ring_index];

    if (tex.first != index || !tex.second) {
        // Upload texture, update index stamp
        tex.first = index;
        tex.second = gl::Texture::create((unsigned char *) frame.pixels, GL_BGRA, buffer.kWidth, buffer.kHeight);
    }
    
    gl::enableAlphaBlending();
    gl::color(1.0f, 1.0f, 1.0f, alpha);

    gl::draw(tex.second);

    unsigned num_points = min<unsigned>(0+TrackingBuffer::kMaxTrackingPoints, frame.num_points);

    // Black outline
    gl::color(0.0f, 0.0f, 0.0f, alpha);
    for (unsigned i = 0; i < num_points; i++) {
        auto& point = frame.points[i];
        Vec2f pos(point.x, point.y);
        gl::drawSolidCircle(pos, 3.0f);
    }

    // White dot
    gl::color(1.0f, 1.0f, 1.0f, alpha);
    for (unsigned i = 0; i < num_points; i++) {
        auto& point = frame.points[i];
        Vec2f pos(point.x, point.y);
        gl::drawSolidCircle(pos, 1.8f);
    }

    // Vector
    for (unsigned i = 0; i < num_points; i++) {
        auto& point = frame.points[i];
        Vec2f pos(point.x, point.y);
        Vec2f delta(point.dx, point.dy);
        gl::drawLine(pos-delta, pos);
    }
}

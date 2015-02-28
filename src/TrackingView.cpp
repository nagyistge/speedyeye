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
        drawFrame(buffer, i, 0.2f);
    }
    
    drawTotalMotion(buffer);
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
    glPointSize(5);
    gl::begin(GL_POINTS);
    for (unsigned i = 0; i < num_points; i++) {
        if (frame.points[i].age >= TrackingBuffer::kPointTrialPeriod) {
            auto& point = frame.points[i];
            Vec2f pos(point.x, point.y);
            gl::vertex(pos);
        }
    }
    gl::end();

    // White vector
    gl::lineWidth(1);
    gl::color(1.0f, 1.0f, 1.0f, 1.0f);
    for (unsigned i = 0; i < num_points; i++) {
        if (frame.points[i].age >= TrackingBuffer::kPointTrialPeriod) {
            auto& point = frame.points[i];
            Vec2f pos(point.x, point.y);
            Vec2f delta(point.dx, point.dy);
            gl::drawLine(pos-delta, pos);
        }
    }
}

static float fmod_positive(float n, float d)
{
    float x = fmod(n, d);
    if (x < 0)
        x += d;
    return x;
}

void TrackingView::drawTotalMotion(TrackingBuffer &buffer)
{
    // Wrap around screen edges
    Vec2f pos(fmod_positive(buffer.data()->header.total_motionX, buffer.kWidth),
              fmod_positive(buffer.data()->header.total_motionY, buffer.kHeight));

    // Pink dot with black outline
    gl::color(0.f, 0.f, 0.f);
    glPointSize(14);
    gl::begin(GL_POINTS);
    gl::vertex(pos);
    gl::end();
    gl::color(1.0f, 0.4f, 1.0f, 1.0f);
    glPointSize(10);
    gl::begin(GL_POINTS);
    gl::vertex(pos);
    gl::end();
}
    
// OpenGL viewer for a TrackingBuffer (c) 2015 Micah Elizabeth Scott
// MIT license

#include "cinder/gl/gl.h"
#include "TrackingView.h"
#include <algorithm>

using namespace std;
using namespace ci;


void TrackingView::setup()
{
    mFrameTextures.clear();
}

void TrackingView::draw(TrackingBuffer &buffer, float alpha)
{
    // Draw all previous frames, in temporal order
    uint32_t frame_counter = buffer.data()->header.frame_counter;
    uint32_t first_frame = max<int64_t>(0, int64_t(frame_counter) - (buffer.kNumFrames - 1));
    for (uint32_t i = first_frame; i < frame_counter; i++) {
        drawFrame(buffer, i, alpha);
    }
}

void TrackingView::drawFrame(TrackingBuffer &buffer, unsigned index, float alpha)
{
    index &= buffer.kNumFrames - 1;
    auto& frame = buffer.data()->frames[index];

    if (mFrameTextures.size() <= index) {
        mFrameTextures.resize(index + 1);
    }
    if (mFrameTextures[index].first != index) {
        // Upload texture
        mFrameTextures[index].first = index;
        mFrameTextures[index].second = Surface((uint8_t*) frame.pixels, buffer.kWidth, buffer.kHeight, buffer.kWidth * 4, RGBA);
    }
    std::vector<std::pair<uint32_t, ci::gl::TextureRef>> mFrameTextures;

    Surface surf(buffer.data()->frames[index].pixels, buffer.kWidth, buffer.kHeight, true,)
    
    gl::enableAlphaBlending();
    gl::color(1.0f, 1.0f, 1.0f, alpha);

    
    gl::draw(mFr)
}

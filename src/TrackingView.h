// OpenGL viewer for a TrackingBuffer (c) 2015 Micah Elizabeth Scott
// MIT license

#pragma once

#include "cinder/gl/gl.h"
#include <vector>
#include "TrackingBuffer.h"

class TrackingView {
public:
    void setup();
    void draw(TrackingBuffer &buffer);
    void drawFrame(TrackingBuffer &buffer, unsigned index, float alpha = 1.0f);
    void drawTotalMotion(TrackingBuffer &buffer);
    
private:
    std::vector<std::pair<uint32_t, ci::gl::TextureRef>> mFrameTextures;
};
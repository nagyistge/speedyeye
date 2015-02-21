// FIFO buffer for frames and tracked points (c) 2015 Micah Elizabeth Scott
// MIT license

#pragma once

#include <iostream>
#include <fstream>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include "cinder/Color.h"
#include "ps3eye.h"


class TrackingBuffer {
public:
    TrackingBuffer();
    TrackingBuffer(const char *filename);
    
    // Number of frames the buffer can hold, as a power of two
    static const unsigned kNumFramesLog2 = 5;
    static const unsigned kNumFrames = 1 << kNumFramesLog2;

    static const unsigned kWidth = 320;
    static const unsigned kHeight = 240;
    static const unsigned kFPS = 187;
    
    static const unsigned kMaxTrackingPoints = 4 * 1024;
    
    struct Header_t {
        uint32_t frame_counter;
        float min_point_quality;
        uint8_t camera_autogain;
        uint8_t camera_gain;
        uint8_t camera_exposure;
        uint8_t camera_sharpness;
        uint8_t camera_hue;
        uint8_t camera_awb;
        uint8_t camera_brightness;
        uint8_t camera_contrast;
        uint8_t camera_blueblc;
        uint8_t camera_redblc;
        uint8_t camera_flip_h;
        uint8_t camera_flip_v;
    };
    
    struct Point_t {
        float x, y;             // Current location, subpixel accuracy
        float dx, dy;           // Distance from previous location, or zero if new
        uint32_t age;           // Number of previous frames this point was seen on
        uint32_t last_index;    // Index in points[] from the last frame, if age != 0
    };

    struct Frame_t {
        double timestamp;
        uint32_t num_points;
        uint32_t pixels[kWidth * kHeight];      // Luminance + RGB
        Point_t points[kMaxTrackingPoints];

        void init(double timestamp);
        void trackPoints(const Frame_t &previous);
        bool newPoint(const Frame_t &previous);
        ci::Color8u getPixel(int x, int y) const;
    };
    
    struct SharedMemory_t {
        Header_t header;
        Frame_t frames[kNumFrames];
    };

    SharedMemory_t* data() {
        return static_cast<SharedMemory_t*>(mMappedRegion.get_address());
    }
    
private:
    boost::interprocess::file_mapping mFileMapping;
    boost::interprocess::mapped_region mMappedRegion;
};
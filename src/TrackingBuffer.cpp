// FIFO buffer for frames and tracked points (c) 2015 Micah Elizabeth Scott
// MIT license

#include "TrackingBuffer.h"

using namespace std;
using namespace boost::interprocess;


TrackingBuffer::TrackingBuffer()
{}

TrackingBuffer::TrackingBuffer(const char *filename)
{
    // Make an empty file of the right size
    filebuf fb;
    fb.open(filename, ios_base::in | ios_base::out | ios_base::trunc | ios_base::binary);
    fb.pubseekoff(sizeof(SharedMemory_t) - 1, ios_base::beg);
    fb.sputc(0);
    fb.close();

    mFileMapping = file_mapping(filename, read_write);
    mMappedRegion = mapped_region(mFileMapping, read_write);

    // Set up default camera settings
    Header_t& header = data()->header;

    header.tracking_point_limit = 200;
    header.camera_autogain = true;
    header.camera_gain = 20;
    header.camera_exposure = 120;
    header.camera_sharpness = 0;
    header.camera_hue = 143;
    header.camera_awb = true;
    header.camera_brightness = 20;
    header.camera_contrast = 37;
    header.camera_blueblc = 128;
    header.camera_redblc = 128;
    header.camera_flip_h = false;
    header.camera_flip_v = false;
}

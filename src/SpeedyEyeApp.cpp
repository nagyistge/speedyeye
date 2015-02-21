#include "cinder/app/AppNative.h"
#include "cinder/params/Params.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/Utilities.h"
#include "cinder/Thread.h"

#include "yuv422.h"
#include "ps3eye.h"
#include "TrackingBuffer.h"
#include "TrackingView.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ps3eye;


class SpeedyEyeApp : public AppNative {
public:
	void setup();
	void draw();
	void shutdown();
    void prepareSettings(Settings *settings);

	void threadFn();

private:
    params::InterfaceGlRef  mParams;
    PS3EYECam::PS3EYERef    mEye;
    TrackingBuffer          mTrackingBuffer;
    TrackingView            mTrackingView;
    thread                  mThread;
    bool                    mExiting;

    void captureFrame();
};


void SpeedyEyeApp::prepareSettings(Settings *settings)
{
    settings->setTitle("SpeedyEye");
    settings->setFrameRate(60.0f);
}

void SpeedyEyeApp::setup()
{
	mExiting = false;
    mTrackingBuffer = TrackingBuffer("tracking-buffer.bin");
    
    std::vector<PS3EYECam::PS3EYERef> devices(PS3EYECam::getDevices());
    if (!devices.size()) {
        console() << "No camera detected" << endl;
        exit(1);
    }

    mEye = devices.at(0);
    bool res = mEye->init(TrackingBuffer::kWidth, TrackingBuffer::kHeight, TrackingBuffer::kFPS);
    if (!res) {
        console() << "Failed to initialize camera" << endl;
        exit(1);
    }

    mTrackingView.setup();

    mThread = thread(bind(&SpeedyEyeApp::threadFn, this));

    mParams = params::InterfaceGl::create(getWindow(), "Camera Settings", toPixels(Vec2i(250, 225)));

    mParams->addParam("Auto gain", (bool*)&mTrackingBuffer.data()->header.camera_autogain);
    mParams->addParam("Gain", &mTrackingBuffer.data()->header.camera_gain).min(0).max(255);
    mParams->addParam("Exposure", &mTrackingBuffer.data()->header.camera_exposure).min(0).max(255);
    mParams->addParam("Sharpness", &mTrackingBuffer.data()->header.camera_sharpness).min(0).max(255);
    mParams->addParam("Hue", &mTrackingBuffer.data()->header.camera_hue).min(0).max(255);
    mParams->addParam("Auto white balance", (bool*)&mTrackingBuffer.data()->header.camera_awb);
    mParams->addParam("Brightness", &mTrackingBuffer.data()->header.camera_brightness).min(0).max(255);
    mParams->addParam("Contrast", &mTrackingBuffer.data()->header.camera_contrast).min(0).max(255);
    mParams->addParam("Blue balance", &mTrackingBuffer.data()->header.camera_blueblc).min(0).max(255);
    mParams->addParam("Red balance", &mTrackingBuffer.data()->header.camera_redblc).min(0).max(255);
    mParams->addParam("Flip H", (bool*)&mTrackingBuffer.data()->header.camera_flip_h);
    mParams->addParam("Flip V", (bool*)&mTrackingBuffer.data()->header.camera_flip_v);
}

void SpeedyEyeApp::threadFn()
{
    mEye->start();

	while (!mExiting) {
        if (!PS3EYECam::updateDevices()) {
            console() << "Stopping because of device error" << endl;
            break;
        }

        #define CAMERA_PARAM(field, getter, setter) \
            if (mEye->getter() != mTrackingBuffer.data()->header.field) { \
                mEye->setter(mTrackingBuffer.data()->header.field); \
                mTrackingBuffer.data()->header.field = mEye->getter(); \
            }
    
        CAMERA_PARAM(camera_autogain, getAutogain, setAutogain);
        CAMERA_PARAM(camera_gain, getGain, setGain);
        CAMERA_PARAM(camera_exposure, getExposure, setExposure);
        CAMERA_PARAM(camera_sharpness, getSharpness, setSharpness);
        CAMERA_PARAM(camera_hue, getHue, setHue);
        CAMERA_PARAM(camera_awb, getHue, setHue);
        CAMERA_PARAM(camera_brightness, getBrightness, setBrightness);
        CAMERA_PARAM(camera_contrast, getContrast, setContrast);
        CAMERA_PARAM(camera_blueblc, getBlueBalance, setBlueBalance);
        CAMERA_PARAM(camera_redblc, getRedBalance, setRedBalance);

        if (mEye->getFlipH() != mTrackingBuffer.data()->header.camera_flip_h ||
            mEye->getFlipV() != mTrackingBuffer.data()->header.camera_flip_v) {
            mEye->setFlip(mTrackingBuffer.data()->header.camera_flip_h = !!mTrackingBuffer.data()->header.camera_flip_h,
                          mTrackingBuffer.data()->header.camera_flip_v = !!mTrackingBuffer.data()->header.camera_flip_v);
        }

        #undef CAMERA_PARAM

        if (mEye->isNewFrame()) {
            captureFrame();
        }
    }

    mEye->stop();
}

void SpeedyEyeApp::shutdown()
{
	mExiting = true;
	mThread.join();
}

void SpeedyEyeApp::draw()
{
    gl::clear();
    gl::setMatricesWindow( getWindowWidth(), getWindowHeight() );

    mTrackingView.draw(mTrackingBuffer);

    mParams->draw();
}

void SpeedyEyeApp::captureFrame()
{
    auto shm = mTrackingBuffer.data();
    uint32_t frame_counter = shm->header.frame_counter;
    auto& newFrame = shm->frames[frame_counter & (TrackingBuffer::kNumFrames-1)];

    newFrame.timestamp = getElapsedSeconds();
    newFrame.num_points = 0;

    yuv422_to_rgba(mEye->getLastFramePointer(), mEye->getRowBytes(),
                   (uint8_t*) newFrame.pixels,
                   TrackingBuffer::kWidth, TrackingBuffer::kHeight);

    // New frame is now fully written
    shm->header.frame_counter = frame_counter + 1;
}


CINDER_APP_NATIVE( SpeedyEyeApp, RendererGl )

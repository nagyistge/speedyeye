#include "cinder/app/AppNative.h"
#include "cinder/params/Params.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/Utilities.h"
#include "cinder/Thread.h"
#include <mutex>

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
	SpeedyEyeApp();

	void setup();
	void draw();
	void shutdown();
    void prepareSettings(Settings *settings);

	void threadFn();

private:
    params::InterfaceGlRef  mParams;
    PS3EYECam::PS3EYERef    mEye;
    fs::path                mTrackingBufferPath;
    TrackingBuffer          mTrackingBuffer;
    TrackingView            mTrackingView;
    thread                  mThread;
    bool                    mExiting;
	bool                    mInitialized;
    float                   mAverageCameraFps;
    float                   mTrackingTime;
    float                   mMaxTrackingTime;
    int                     mCurrentNumPoints;
    string                  mErrorString;
	mutex                   mErrorMutex;
    
    void captureFrame();
    void newTrackingPoint();
};


SpeedyEyeApp::SpeedyEyeApp()
	: mInitialized(false)
{}

void SpeedyEyeApp::prepareSettings(Settings *settings)
{
    settings->setTitle("SpeedyEye");
    settings->setFrameRate(60.0f);
    settings->setWindowSize(800, 600);
}

void SpeedyEyeApp::setup()
{
	mExiting = false;
    mAverageCameraFps = 0.0f;
    mCurrentNumPoints = 0;
    mTrackingTime = 0.0f;
    mMaxTrackingTime = 0.9f;

    mTrackingBufferPath = getSaveFilePath("tracking-buffer.bin");

    if (!mTrackingBuffer.open(mTrackingBufferPath.string().c_str())) {
        mErrorString = "Failed to create tracking buffer file";
        return;
    }
    
    std::vector<PS3EYECam::PS3EYERef> devices(PS3EYECam::getDevices());
    if (!devices.size()) {
        mErrorString = "No camera detected.  (Sorry, you'll need to restart the app to try again)";
        return;
    }

    mEye = devices.at(0);
    bool res = mEye->init(TrackingBuffer::kWidth, TrackingBuffer::kHeight, TrackingBuffer::kFPS);
    if (!res) {
        mErrorString = "Failed to initialize camera?";
        return;
    }

    mTrackingView.setup();

    mThread = thread(bind(&SpeedyEyeApp::threadFn, this));

    mParams = params::InterfaceGl::create(getWindow(), "Camera Settings", toPixels(Vec2i(250, 350)));

    mParams->addParam("Camera FPS", &mAverageCameraFps, "readonly=true");
    mParams->addParam("Tracking points", &mCurrentNumPoints, "readonly=true");
    mParams->addParam("Tracking time", &mTrackingTime, "readonly=true");
    mParams->addParam("Max tracking time", &mMaxTrackingTime).min(0.f).max(1.f).step(0.01f);
    mParams->addSeparator();
    mParams->addParam("Flip H", (bool*)&mTrackingBuffer.data()->header.camera_flip_h);
    mParams->addParam("Flip V", (bool*)&mTrackingBuffer.data()->header.camera_flip_v);
    mParams->addParam("Tracking point quality", &mTrackingBuffer.data()->header.min_point_quality).min(0.001f).max(1.f).step(0.001f);
    mParams->addSeparator();
    mParams->addParam("Auto gain", (bool*)&mTrackingBuffer.data()->header.camera_autogain);
    mParams->addParam("Gain", &mTrackingBuffer.data()->header.camera_gain).min(0).max(255);
    mParams->addParam("Exposure", &mTrackingBuffer.data()->header.camera_exposure).min(0).max(255);
    mParams->addParam("Sharpness", &mTrackingBuffer.data()->header.camera_sharpness).min(0).max(255);
    mParams->addParam("Brightness", &mTrackingBuffer.data()->header.camera_brightness).min(0).max(255);
    mParams->addParam("Contrast", &mTrackingBuffer.data()->header.camera_contrast).min(0).max(255);
    mParams->addSeparator();
    mParams->addParam("Auto white balance", (bool*)&mTrackingBuffer.data()->header.camera_awb);
    mParams->addParam("Blue balance", &mTrackingBuffer.data()->header.camera_blueblc).min(0).max(255);
    mParams->addParam("Red balance", &mTrackingBuffer.data()->header.camera_redblc).min(0).max(255);
    mParams->addParam("Hue", &mTrackingBuffer.data()->header.camera_hue).min(0).max(255);

	mInitialized = true;
}

void SpeedyEyeApp::threadFn()
{
    mEye->start();

	while (!mExiting) {
        if (!PS3EYECam::updateDevices()) {
			lock_guard<mutex> lock(mErrorMutex);
			mErrorString = "Stopping because of USB device error";
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
    if (mThread.joinable()) {
        mThread.join();
    }
}

void SpeedyEyeApp::draw()
{
	{
		lock_guard<mutex> lock(mErrorMutex);
		if (mErrorString.length() > 0) {
			gl::clear(Color(0.2f, 0.f, 0.f));
			gl::enableAlphaBlending();
			gl::drawStringCentered(mErrorString, getWindowSize() * 0.5f);
			return;
		}
	}

    gl::clear();

	if (mInitialized) {
		// Coordinate system to match the camera resolution
		gl::setMatricesWindow(TrackingBuffer::kWidth, TrackingBuffer::kHeight);
		mTrackingView.draw(mTrackingBuffer);

		// Pixel coordinates
		gl::setMatricesWindow(getWindowWidth(), getWindowHeight());

		// Reminder of the buffer path we're using
		gl::color(0.7f, 1.f, 0.8f);
		gl::enableAlphaBlending();
		gl::drawStringCentered(mTrackingBufferPath.string().c_str(), Vec2i(getWindowWidth() / 2, getWindowHeight() - 20));

		mParams->draw();
	}
}

void SpeedyEyeApp::captureFrame()
{
    auto shm = mTrackingBuffer.data();
    uint32_t frame_counter = shm->header.frame_counter;
    auto& newFrame = shm->frames[frame_counter & (TrackingBuffer::kNumFrames-1)];

    double timeA = getElapsedSeconds();
    newFrame.init(timeA);

    yuv422_to_rgbl(mEye->getLastFramePointer(), mEye->getRowBytes(),
                   (uint8_t*) newFrame.pixels,
                   TrackingBuffer::kWidth, TrackingBuffer::kHeight);

    if (frame_counter > 0) {
        // There exists a previous frame, we can do tracking
        auto& prevFrame = shm->frames[(frame_counter - 1) & (TrackingBuffer::kNumFrames-1)];

        newFrame.trackPoints(prevFrame);
        double timeB = getElapsedSeconds();

        shm->header.total_motionX += newFrame.motionX;
        shm->header.total_motionY += newFrame.motionY;
        
        double trackingTime = (timeB - timeA) * TrackingBuffer::kFPS;
        mTrackingTime = trackingTime;
        
        if (trackingTime < mMaxTrackingTime && newFrame.num_points < TrackingBuffer::kMaxTrackingPoints) {
            newFrame.newPoint(prevFrame);
        }

        mCurrentNumPoints = newFrame.num_points;
    }
    
    // New frame is now fully written
    shm->header.frame_counter = frame_counter + 1;
    mAverageCameraFps = shm->header.frame_counter / getElapsedSeconds();
}


CINDER_APP_NATIVE( SpeedyEyeApp, RendererGl )

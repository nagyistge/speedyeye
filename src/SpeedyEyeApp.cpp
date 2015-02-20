#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/Utilities.h"
#include "cinder/Thread.h"

#include "yuv422.h"
#include "ps3eye.h"

using namespace ci;
using namespace ci::app;
using namespace std;


class PS3EYECaptureApp : public AppBasic {
  public:
	void setup();
	void update();
	void draw();
	void shutdown();

	void eyeUpdateThreadFn();

    ps3eye::PS3EYECam::PS3EYERef eye;

	bool					mShouldQuit;
	thread                  mThread;

	gl::Texture mTexture;
	uint8_t *frame_bgra;
	Surface mFrame;

	// mesure cam fps
	Timer					mTimer;
	uint32_t				mCamFrameCount;
	float					mCamFps;
	uint32_t				mCamFpsLastSampleFrame;
	double					mCamFpsLastSampleTime;
};

void PS3EYECaptureApp::setup()
{
    using namespace ps3eye;

	mShouldQuit = false;

    // list out the devices
    std::vector<PS3EYECam::PS3EYERef> devices( PS3EYECam::getDevices() );
	console() << "found " << devices.size() << " cameras" << std::endl;

    if (!devices.size()) {
        exit(1);
    }

	mTimer = Timer(true);
	mCamFrameCount = 0;
	mCamFps = 0;
	mCamFpsLastSampleFrame = 0;
	mCamFpsLastSampleTime = 0;

    eye = devices.at(0);
    bool res = eye->init(640, 480, 60);
    console() << "init eye result " << res << std::endl;
    eye->start();

    frame_bgra = new uint8_t[eye->getWidth()*eye->getHeight()*4];
    mFrame = Surface(frame_bgra, eye->getWidth(), eye->getHeight(), eye->getWidth()*4, SurfaceChannelOrder::BGRA);
    memset(frame_bgra, 0, eye->getWidth()*eye->getHeight()*4);
    
    // create and launch the thread
    mThread = thread( bind( &PS3EYECaptureApp::eyeUpdateThreadFn, this ) );
}

void PS3EYECaptureApp::eyeUpdateThreadFn()
{
	while( !mShouldQuit )
	{
		bool res = ps3eye::PS3EYECam::updateDevices();
        if (!res) break;

        bool isNewFrame = eye->isNewFrame();
        if (isNewFrame) {
            yuv422_to_rgba(eye->getLastFramePointer(), eye->getRowBytes(), frame_bgra, mFrame.getWidth(), mFrame.getHeight());
        }
    }
}

void PS3EYECaptureApp::shutdown()
{
	mShouldQuit = true;
	mThread.join();
    eye->stop();
	delete[] frame_bgra;
}

void PS3EYECaptureApp::update()
{
    mTexture = gl::Texture( mFrame );
}

void PS3EYECaptureApp::draw()
{
    gl::clear();
    gl::disableDepthRead();
	gl::disableDepthWrite();		
	gl::enableAlphaBlending();

	gl::setMatricesWindow( getWindowWidth(), getWindowHeight() );
	if( mTexture ) {
        gl::draw( mTexture );
	}
}

CINDER_APP_BASIC( PS3EYECaptureApp, RendererGl )

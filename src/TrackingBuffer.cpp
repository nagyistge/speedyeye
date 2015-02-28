// High level sparse LK tracker (c) 2015 Micah Elizabeth Scott
// MIT license

#include "cinder/Rand.h"
#include "CinderOpenCV.h"
#include "TrackingBuffer.h"

using namespace std;
using namespace boost::interprocess;
using namespace cv;


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

    header.min_point_quality = 0.1f;
    header.camera_autogain = true;
    header.camera_gain = 20;
    header.camera_exposure = 120;
    header.camera_sharpness = 0;
    header.camera_hue = 143;
    header.camera_awb = true;
    header.camera_brightness = 11;
    header.camera_contrast = 37;
    header.camera_blueblc = 128;
    header.camera_redblc = 128;
    header.camera_flip_h = false;
    header.camera_flip_v = false;
    header.total_motionX = 0.f;
    header.total_motionY = 0.f;
}

void TrackingBuffer::Frame_t::init(double timestamp)
{
    this->timestamp = timestamp;
    num_points = 0;

}

void TrackingBuffer::Frame_t::trackPoints(const Frame_t &previous)
{
    // Run OpenCV's LK tracker, adapting input and output to our Point_t format.
    // This can delete points from frame to frame but never add new points.
    
    Mat imageA(kHeight, kWidth, CV_8UC4, (void*)previous.pixels);
    Mat imageB(kHeight, kWidth, CV_8UC4, (void*)pixels);
    
    vector<Point2f> pointsA, pointsB;
    for (unsigned i = 0; i < previous.num_points; i++) {
        pointsA.push_back(Point2f(previous.points[i].x, previous.points[i].y));
    }

    if (!pointsA.size()) {
        return;
    }
    
    vector<uchar> status;
    vector<float> err;
    cv::TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
    cv::Size subPixWinSize(6,6), winSize(15,15);
    const float minEigThreshold = 0.1f;
    
    calcOpticalFlowPyrLK(imageA, imageB, pointsA, pointsB,
                         status, err, winSize, 3, termcrit, 3, minEigThreshold);

    Point2f numerator(0.f, 0.f);
    float denominator = 0.f;
    
    for (unsigned i = 0; i < status.size(); i++) {
        unsigned this_num_points = num_points;
        const float kDeletePointProbability = 0.001f;
        if (this_num_points < kMaxTrackingPoints && status[i] && ci::randFloat() > kDeletePointProbability) {
            Point_t& newpoint = points[this_num_points];
            newpoint.x = pointsB[i].x;
            newpoint.y = pointsB[i].y;
            newpoint.dx = newpoint.x - pointsA[i].x;
            newpoint.dy = newpoint.y - pointsA[i].y;
            newpoint.age = previous.points[i].age + 1;
            newpoint.last_index = i;
            num_points = this_num_points + 1;

            // Weighted motion averaging
            float weight = (newpoint.age - kPointTrialPeriod) / err[i];
            numerator.x += newpoint.dx * weight;
            numerator.y += newpoint.dy * weight;
            denominator += weight;
        }
    }
    
    if (denominator > 0.f) {
        motionX = numerator.x / denominator;
        motionY = numerator.y / denominator;
    } else {
        motionX = 0.f;
        motionY = 0.f;
    }
}

ci::Color8u TrackingBuffer::Frame_t::getPixel(int x, int y) const
{
    assert(x >= 0 && x < kWidth && y >= 0 && y < kHeight);
    uint32_t pixel = pixels[x + y * kWidth];
    return ci::Color8u::hex(pixel);
}

bool TrackingBuffer::Frame_t::newPoint(const Frame_t &previous)
{
    /*
     * Look for more points to track. We specifically want to focus on areas that are moving,
     * as a fast alternative to actual background subtraction. We also want to avoid points
     * too near to any existing d ones.
     *
     * To quickly find some interesting points, I further decimate the image into a sparse
     * grid, and look for image corners near the grid points that have the most motion.
     */

    const unsigned kDiscoveryGridSpacing = 5;
    const unsigned kGridWidth = kWidth / kDiscoveryGridSpacing;
    const unsigned kGridHeight = kHeight / kDiscoveryGridSpacing;
    
    vector<bool> gridCoverage;
    gridCoverage.resize(kGridWidth * kGridHeight);
    fill(gridCoverage.begin(), gridCoverage.end(), false);
    
    // Calculate coverage of the discovery grid
    for (unsigned i = 0; i < num_points && i < kMaxTrackingPoints; i++) {
        const Point_t& p = points[i];
        int x = p.x / kDiscoveryGridSpacing;
        int y = p.y / kDiscoveryGridSpacing;
        unsigned idx = x + y * kGridWidth;
        if (idx < gridCoverage.size()) {
            gridCoverage[idx] = true;
        }
    }
    
    // Look for the highest-motion point that isn't already on the grid, ignoring image edges.
    
    Point2f bestPoint = Point2f(0, 0);
    int bestDiff = 0;
    
    for (int y = 1; y < kGridHeight - 1; y++) {
        for (int x = 1; x < kGridWidth - 1; x++) {
            if (!gridCoverage[x + y * kGridWidth]) {
                
                // Random sampling bias, to avoid creating identical tracking points
                const float s = kDiscoveryGridSpacing * 0.4;
                int pixX = x * kDiscoveryGridSpacing + ci::randFloat(-s, s);
                int pixY = y * kDiscoveryGridSpacing + ci::randFloat(-s, s);
                
                int diff2 = getPixel(pixX, pixY).distanceSquared(previous.getPixel(pixX, pixY));
                
                if (diff2 > bestDiff) {
                    bestDiff = diff2;
                    bestPoint = cv::Point2f(pixX, pixY);
                }
            }
        }
    }
    
    if (bestDiff > 0) {
        // Find a good corner near this point
        Mat image(kHeight, kWidth, CV_8UC4, (void*)pixels);
        Mat bgrl[4];
        cv::split(image, bgrl);

        vector<Point2f> newPoint;
        cv::TermCriteria termcrit(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 20, 0.03);
        cv::Size subPixWinSize(6,6), winSize(15,15);
        newPoint.push_back(bestPoint);
        cv::cornerSubPix(bgrl[3], newPoint, subPixWinSize, cv::Size(-1,-1), termcrit);

        Point_t& point = points[num_points];
        point.x = newPoint[0].x;
        point.y = newPoint[0].y;
        point.dx = 0.0f;
        point.dy = 0.0f;
        point.age = 0;
        point.last_index = -1;
        num_points++;
        return true;
    }
    
    return false;
}

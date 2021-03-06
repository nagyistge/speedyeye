SpeedyEye
=========

This is a simple way to add high-speed motion tracking to your interactive art. It communicates with the PS3 Eye camera using the [inspirit/PS3EYEDriver](https://github.com/inspirit/PS3EYEDriver) driver. There's a simple built-in GUI for adjusting camera parameters and showing the buffer of recent capture data in an *onion-skin* style.

* One camera supported for now
* The camera runs at 320x240 **187 frames per second** mode only.
* Every frame is precisely timestamped as soon as it is received over USB
* Every frame is converted to RGBA and stored in a **shared memory ring buffer**
* Additionally, the OpenCV implementation of [Lucas-Kanade sparse optical flow](http://en.wikipedia.org/wiki/Lucas%E2%80%93Kanade_method) runs in real-time on each frame, automatically finding and tracking as many points as it can with the available CPU power.
* The tracking points and their motion, with subpixel accuracy, are also stored in this ring buffer
* Total motion is integrated using the same technique used by [Ecstatic Epiphany](https://github.com/scanlime/ecstatic-epiphany)'s motion tracking

To Do
-----

* High level client APIs still yet to be written. So far, there's a very low-level Processing example.

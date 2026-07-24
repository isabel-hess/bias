BIAS
*****

BIAS is a software application for recording video from IEEE 1394 and USB3
Cameras.  BIAS was intially designed as image acquisition software for
experiments in animial behavior. For example, recording the behavior of fruit
flies in a walking arena. 

|

.. figure:: https://bitbucket.org/iorodeo/bias/raw/default/images/bias_charlie.png
    :scale: 100 %
    :alt: alternate text
    :align: center



Features
---------

BIAS provides the following features: 

* Control of camera properties (brightness, shutter, gain, etc.)
* Timed video recordings
* Support for a variety of video file formats (avi,fmf, ufmf, mjpg, raw image
* files) etc. 
* JSON based configuration files 
* External control via http commands - start/stop recording, set camera
* configuration etc.
* A plugin system for machine vision applications and for controlling external
* instrumentation
* Multiple cameras
* Image alignment tools
* Cross platform - windows, linux


Camera backends
---------------

BIAS talks to cameras through pluggable backends selected at build time:

* ``fc2``    - FlyCapture2 (Point Grey / FLIR, IEEE 1394 / USB)
* ``dc1394`` - libdc1394 (IEEE 1394)
* ``spin``   - Spinnaker (FLIR, USB3)
* ``arena``  - Lucid Vision Labs (GigE Vision), via the Arena SDK C API

The Lucid Arena backend is the most recent addition and identifies GigE
cameras by IP address. See the documentation below.


Documentation
-------------

Project notes (upstream): http://public.iorodeo.com/notes/bias/

Local documentation lives in the `docs/ <docs/>`_ directory:

* `docs/README.md <docs/README.md>`_ - documentation index
* `docs/lucid-arena-backend.md <docs/lucid-arena-backend.md>`_ - the Lucid
  Arena (GigE) backend and the changes made to build BIAS with a modern
  MSYS2 toolchain
* `docs/building-on-windows.md <docs/building-on-windows.md>`_ -
  installation and compilation guide (MSYS2 / MinGW UCRT64)




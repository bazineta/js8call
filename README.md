# JS8Call

This is, in the flavor of `WSJTX-improved`, an 'improved' version of the original JS8Call, the source
code for which can be found at: https://bitbucket.org/widefido/js8call/src/js8call/

I am not the original author, and have no desire to create a fork, add new features, etc. My motivation
was to have a native version of JS8Call that would run on my Apple Silicon Mac, using a current version
of the Qt and Hamlib libraries. Along the way, I discovered and corrected a few bugs, and made some minor
visual improvements to the UI.

I did stumble over a very signficant issue in the way that the underlying WSJTX code performed Fortran
string passing; this bug affects code compiled with GNU Fortran >= 8.0 and a non-GNU C++ compiler, such
as `clang`. This bug exhibits as seemingly random behavior, often things like inexplicable crashes in
azimuth and distance calculation. The fix has been provided to the WSJTX team, and it's present in
WSJTX-improved starting in the test version of August 29th, 2024.

Anyway.....that's what this does; that's all this does. It's not intended to be anything but a vehicle
by which to provide my changes to the original author.

Allan Bazinet, W6BAZ

# Compiling on OSX

1. Obtain the current version of GNU Fortran, as of this writing 14.2, from https://github.com/fxcoudert/gfortran-for-macOS,
   and install it; you'll end up with an installation in `/usr/local/gfortran`.

2. Obtain the current version of CMake from https://cmake.org/download/. Both source and binary
   distributions are available; I usually obtain the source distribution and compile, but whatever
   floats your boat is fine.

3. Create a directory in which to build up the dependencies; the name of this directory doesn't matter,
   but must be used consistently, e.g., `/Users/<username>/Development/js8libs`. everything we require
   as a dependency will be installed to this path. For purposes of clarity in this document, we'll use:
   ```
   /Users/alb/Development/js8libs
   ```
   Modify as appropriate for your own username and directory structure.

4. Obtain the current daily snapshot of the Hamlib code from https://n0nb.users.sourceforge.net/; at
   present, this will be some master build of 4.6. Unpack the source distribution and install it to the
   dependencies directory:
   ```
   ./configure --prefix=/Users/alb/Development/js8libs
   make
   make install
   ```

5. Obtain the current release of fftw, presently version 3.3.10, from https://www.fftw.org/. Unpack the
   source distribution and install it to the dependencies directory:
   ```
   ./configure CFLAGS=-mmacosx-version-min=11.0 \
               --prefix=/Users/alb/Development/js8libs \
               --enable-single \
               --enable-threads
   make
   make install
   ```

6. Obtain the current release of boost, presently 1.86.0, from https://www.boost.org/. Unpack the source
   distribution and install it to the dependencies directory:
   ```
   ./bootstrap.sh --prefix=/Users/alb/Development/js8libs
   ./b2 -a -q
   ./b2 -a -q install
   ```

7. Obtain and install Qt 6, using the documentation here: https://doc.qt.io/qt-6/macos-building.html.
   When configuring, use the usual -prefix option to install the built products into the dependencies
   directory.

8. Optionally, obtain and install Qt Creator: https://wiki.qt.io/Building_Qt_Creator_from_Git. By
   now, you should be familiar with use of the dependencies directory, so we'll leave that as an
   exercise for the student. You got this.

9. Create a build directory, typically under this source tree, and run `cmake` to configure the build,
   followed by a `make install`.
   ```
   mkdir build
   cd build
   cmake -DCMAKE_PREFIX_PATH=/Users/alb/Development/js8libs \
         -DCMAKE_Fortran_COMPILER=/usr/local/gfortran/bin/gfortran \
         -DCMAKE_BUILD_TYPE=Release ..
   make install
   ```
   There should be a small handful of Fortran warnings; it's practically impossible to get rid of all
   of them; some are false positives, and some just reflect the fact that it's a language with a lot
   of history behind it, and certain things, like using the same array for both in and out intents,
   will tend to upset it, even if that usage is not a problem. If all goes well, you should end up
   with a `js8call` in the build directory. Test using:
   ```
   open ./js8call.app
   ```
   Once you're satisfied with the test results, copy the `js8call` application to `/Applications`.




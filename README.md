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

# Notable Changes

- Fixed the aforementioned string passing bug; crashes from this were typically random, but it'd
  usually abort with a segmentation fault in azdist(), sometimes in genjs8().
- The subtractjs8() routine used a common block for large, parameterized arrays. I'd see crashes
  here frequently; it's been a long time since I did any serious development in Fortran, but if
  memory serves, this is a dark corner where in terms of how large that common block is, first
  one in wins, and you hope that it's the larger of the parameterized invocations. If instead,
  the smaller one is dominant, then you'll either trash memory after the common block or crash
  with a segfault. Changed this to use allocations instead of a common block, which seems like
  a solid solution, but again, been a long time since I did this sort of thing in anger.
- Fixed a memory leak present when a custom push to talk command was configured. I haven't gone
  looking for memory leaks with the analysis tools; that one just happened to stand out.
- Fixed a buffer overflow in the code that prints the last transmit.
- Ported to Qt6, which changed the audio classes in a major way. Fortunately the wsjtx-improved
  team had been down this road already, and had dealt with most of the changes needed to the
  audio stuff.
- Incorporated a number of fixes from the upstream WSJTX Fortran library. Key among these was a
  change to how VHF contest mode was handled in some of the common routines; in short, they made
  things a lot simpler, which eliminated the need for passing a lot of dummy parameters just to
  keep them happy.
- Eliminated a large number of unused dummy parameters in the JS8 decoder; warning spam from the
  Fortran compiler was a bit distracting, and hid actual issues, such as uninitialized variables
  in the optimized code.
- Did a bit of work with alignment of data in the tables for better presentation.
- The audio input VU meter looked off to me, as if the scale was on the wrong side; flipped it to
  be next to the level and peak hold, which looks more normal to me. Perhaps just a taste thing;
  easy enough to change it back if necessary.
- Changed the waterfall scale drawing methodology slightly to avoid the scale font looking
  pixelated on high-DPI displays. Fonts will still be pixelated in the waterfall display, but
  it's arguably an effect there, like a Tektronix scope. The plot drawing code uses a number of
  intermediate pixmaps, so dealing with the font in the waterfall is complicated; it'd be nice
  to move this to the GL approach taken by SDRangel.
- Hovering on the waterfall display now shows the frequency as a tooltip.
- Converted the boost library to an out-of-tree build.
- Updated the sqlite and qcustomplot libraries.
- Updated the Hamlib library to the current 4.6 snapshot, which provides support for many radios
  not previously supported, e.g., the 705.
- Updated the Fortran code generation to use 2018 semantics when dealing with large arrays that
  exceed available stack space, i.e., use allocation, rather than a hidden static block, as this
  tends to be less surprising behavior in modern usage.

While Qt6 by default will display using a platform-specific style, I've not yet done much work to
deal with changes required there (e.g., platform-specific stylesheet changes, where custom styles
are in use), so for the moment it continues to default to the previous Windows look and feel. One
can override this at runtime with the usual command line parameter to try an alternate style, e.g.,
```
open ./js8call.app --args -style fusion
open ./js8call.app --args -style macos
```

While I've done my best here to avoid causing problems to any other platforms, I've only tested
this on OSX. In theory, I haven't broken anything, but in practice, I haven't tried it out. Any
issues will likely be in the CMake setup, which I find somewhat akin to hostage negotiation.

The earliest version of OSX that Qt6 supports is 11.0. It's set up to compile and link to run
on 11.0 or later, but I've only tested it on 14.6.

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
   When configuring, use the usual `-prefix` option to install the built products into the dependencies
   directory.

8. Optionally, obtain and install Qt Creator: https://wiki.qt.io/Building_Qt_Creator_from_Git. By
   now, you should be familiar with use of the dependencies directory, so we'll leave that as an
   exercise for the student. You got this.

   While not strictly necessary, Qt Creator certainly makes debugging relatively simple, so I'd
   go for it, frankly.

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
   with a `js8call` application in the build directory. Test using:
   ```
   open ./js8call.app
   ```
   Once you're satisfied with the test results, copy the `js8call` application to `/Applications`.

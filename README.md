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

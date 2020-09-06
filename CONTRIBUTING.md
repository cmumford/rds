# Contributing to the RDS Library.

Contributions to this library are welcome. All submissions should follow
the guidelines below.

## Guidelines

* The implementation should conform to the specification and strive to
  reduce the amount of work done by the application.
* All code must to comply with the
  [Chromium C++ style guide](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md).
  When coding run `make format` to automatically apply this style before
  uploading a change.
* Only the basic RDS is decoded in this library. Extensions, like
  ODA, are handed off to the host if registered.
* Display values, for example `PTY`, are not decoded to strings for display.
  These can be region/language specific, and are the responsibility of
  the application to convert to the proper display value.
* Public functions should include a Mongoose OS equivalent.
* Public functions must be documented.

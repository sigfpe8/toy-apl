## Welcome to toyAPL (WIP)!

ToyAPL implements the basic features of APL\360 and a few things from APL.SV (the format and execute functions). I am a C programmer with no APL experience. This implementation is my attempt to learn APL while also trying to examine it with the implementers' eye. So, obviously it is naïve and inefficient in many ways, but, as is often said, the journey is more important than the destination.

I am aware of other implementations with available source code (e.g. GNU APL), but examining these implementations would deprive me of the joys of discovery and of making my own mistakes so I have not looked at them.

This is still work in progress.

## Platforms

The goal is to make it available on macOS, Linux and Windows but for now it is building only on macOS.

## Unicode

All terminal input/output is done using UTF-8. This is made easier on macOS by using Dyalog's Alt keyboard driver, which provides easy typing for all APL characters using Alt and Alt-Shift keys. During input, the Unicode characters are mapped to byte tokens but strings retain their UTF-8 encoding.

## The function editor

## Limitations

## Not implemented

## Future work

* Use of 16-bit UTF-16 Unicode characters for strings
* Better handling of floating-point comparison and division
* Implement internal integer datatype (currently only FP)
* Nested arrays
* D-fns
* A C++ rewrite


## Welcome to toyAPL (WIP)!

ToyAPL implements the basic features of APL\360 and a few things from APL.SV (the format and execute functions). I am a C programmer with no APL experience. This implementation is my attempt to learn APL while also trying to examine it with the implementers' eye. So, obviously it is naïve and inefficient in many ways, but, as is often said, the journey is more important than the destination.

I am aware of other implementations with available source code (e.g. GNU APL), but examining these implementations would deprive me of the joys of discovery and of making my own mistakes so I have not looked at them.

This is still work in progress.

## Platforms

The goal is to make it available on macOS, Linux and Windows but for now it is building only on macOS.

## Unicode

All terminal input/output is done using UTF-8. This is made easier on macOS by using Dyalog's Alt keyboard driver, which provides easy typing for all APL characters using Alt and Alt-Shift keys. During input, the Unicode characters are mapped to byte tokens but strings retain their UTF-8 encoding.

## The function editor

Traditional APL functions can be created and edited outside toyAPL using a text editor that can encode files in UTF-8. Such files can then be loaded using the system command `)LOAD filename`.

In addition to this mechanism, there is a built-in line editor that can also be used to create and edit functions.

To create a new function, in the REPL type `∇` followed by the function header. For example

    ∇ ret ← x myfun y ; a; b; c

To edit an existing function, type `∇` followed just by the function name:

    ∇ myfun

In both cases, after editing the function, type `∇` again to exit the editor and return to the REPL.

### Valid editor commands

Most of these commands behave as in other APL implementations, but some are different. A major difference from other editors is that toyAPL doesn't use fractional line numbers. To insert a new line before or after an existing line, use the `<` and `>` commands. The line numbers are always automatically adjusted.

* [⎕]       Display all lines, insert after end
* [N⎕]      Display from line N to end, insert after end
* [⎕N]      Display from line 1 to line N, insert after line N
* [M⎕N]     Display from line M to line N, insert after line N
* [N]       Replace line N
* [!N]      Delete line N (bang!)
* [<N]      Insert before line N
* [>N]      Insert after line N

It is possible to add a command in the invocation to edit a function. For example

    ∇ myfun [⎕]

will open `myfun` for editing and display all its lines. If the function has 'N' lines, the editor will then display `[N+1]` and wait for a new line to be entered.

## Limitations

## Not implemented

## Future work

* Use of 16-bit UTF-16 Unicode characters for strings
* Better handling of floating-point comparison and division
* Implement internal integer datatype (currently only FP)
* Nested arrays
* D-fns
* A C++ rewrite


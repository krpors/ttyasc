# ttyasc

A simple `ttyrec` file to [asciinema](https://asciinema.org) transformer.

1. Record a terminal session using `ttyrec`.
1. Run `ttyasc` on the produced file.
1. Redirect the stdout to a file.
1. Play the file using the asciinema player.

# Why?

I was bored a bit, and wanted to write something in C. I discovered
asciinema a while ago, and it reminded me of the `ttyrec` program.
I decided to write a translator to adhere to a part the Unix philosophy:

> "Make each program do one thing well. [...]

Since `ttyrec` does the job well, I felt like writing a simple transformer
from that format to the one asciinema accepts (which is JSON).

# Compiling

The program is written without external dependencies or whatever, so
simply running `make` will do the trick. It will produce one binary,
called `ttyasc`. To make a static executable, run the static target
using `make static`.

# Running

Run `ttyrec` to record a terminal session. Hit `^D` when done. This will
create a file `ttyrecord` by default. Now run the `ttyasc` binary:

    $ ttyasc ./ttyrecord

On the stdout it will print out a JSON structure. This can be used in
the asciinema player by simply redirecting the output to a file.

    $ ttyasc ./ttyrecord > output_somewhere.json

That's all there is to it.

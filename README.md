tpick
=====

`tpick` is a simple, curses-based interactive utility for picking one of a
number of things at the terminal.

![screenshot](https://raw.githubusercontent.com/smblott-github/tpick/master/misc/screenshot1.png)

Here are a couple of examples...

Interactively pick a file (with `zsh`-style globing and read):

    tpick **/*.gpg | read file

Interactively pick from standard input (with `zsh`-style read):

    seq 1000 | tpick -i | read number

With a POSIX shell or bash you can use command expansion:

    number=$(seq 100 | tpick -i)

Execute a command with the picked thing as an argument:

    ls -1 *.coffee | tpick -i coffee -c

`tpick` works like `dmenu` or `slmenu`: just start typing characters, and only entries containing those characters are displayed.  Matching uses `fnmatch` and is smartcase (if your `fnmatch` supports GNU extensions).

Use `ENTER` to exit and write the selected thing (the one at the top of the list) to standard output.  You can also exit (and fail) using either `ESCAPE`, `Control-C` or `qq`.  You never search for two consecutive `q` characters, right?

There are more details in the manual page.

precise pick (ppick)
=====
ppick reads a list of choices from stdin (or command line) and outputs the selected choice to stdout. The choices can be filtered interactively before picking just by typing. Here, it differs from the well-known [pick](https://github.com/mptre/pick) command in that matches are exact, not fuzzy. This makes the filtered list shorter and less confusing than the results returned by [pick](https://github.com/mptre/pick), especially if the list has a lot of punctuation. Also ppick's source code is way simpler than pick, making it an easier base for other pick-clones. 

Here are some of its (simple, but) great features:

- reads choices from stdin by default, but with `-l` it can read it from the command line.
- Hit `SPACE` to add the `*` wildcard; so, super-fast matching of multiple parts of the thing you're looking for.
- Two consecutive `q` characters quits (by default); super easy and quick to type.
- Pick your favorite with one key, `;`.  Use the `-f` option to set your favorite text.  Then, when `;` is pressed, that text is added to the search.  I use this for returning to my "home" tmux session.
- And smartcase, of course.

And here are a couple of examples...

    cd /etc
    ppick *

Then type `de`, and the screen looks like...

![screenshot](https://raw.githubusercontent.com/smblott-github/tpick/master/misc/screenshot1.png) 
(note, screenshot is dated to how things looked under tpick, though it's not that different now).

Interactively pick a file (with `zsh`-style globing):

    ppick -l **/*.gpg | xargs less

Interactively pick from standard input (with `zsh`-style read):

    seq 1000 | ppick | read number

With a POSIX shell or bash you can use command expansion:

    number=$(seq 100 | ppick )

`ppick` works like `dmenu` or `slmenu`: just start typing characters, and only entries containing those characters are displayed.  Matching uses `fnmatch` and is smartcase (if your `fnmatch` supports GNU extensions).

Use `ENTER` to exit and write the selected thing (the one at the top of the list) to standard output.  You can also exit (and fail) using either `ESCAPE`, `Control-C` or `qq`.  You never search for two consecutive `q` characters, right?

There are more details in the manual page.

History
=====
`ppick` is a fork of (tpick)[https://github.com/smblott-github/tpick], which was chosen as a base due to it's simple and easy to understand code base. Some of the code changes increase the readability (better variable names), as well as functionality (faster screen updates), but the interface, particularly at the command line, is quite different from tpick, and closer to pick. Thus the motivation to fork from tpick. 

//  gcc ppick.c -lncurses -o ppick

// precise pick version 1.0 (c) alan robinson (https://alantechreview.blogspot.com/)
// in most ways a simpler version of pick, but without the fuzzy matching that 
// can get way out of hand in pick, fzf, etc. 
// based on tpick version 1.0.0 from https://github.com/smblott-github/tpick
// IMHO much improved over tpick: better variable names, improved scrolling, 
// faster/less flickery display but enough UI changes to merit a fork & new name


#include <ncurses.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Try to pick up GNU extensions for <fnmatch.h>.
#if defined(__linux__)
#define _GNU_SOURCE
#endif

#include <fnmatch.h>

// FNM_CASEFOLD is a GNU extension.  We get smartcase if we have FNM_CASEFOLD,
// otherwise matching is case sensitive.
#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD 0
#endif

#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#define middle(a,b,c) max(a, min(b, c));

/* **********************************************************************
 * Usage.
 */

char *usage_message[] =
   {
      "ppick [OPTIONS...] -l [THINGS...]",
      "OPTIONS:",
      "   -l      : read things from the command line (whitespace seperated)",
      "   -w      : read things from standard input (whitespace separated)",
      "   -p TEXT : prepend TEXT to fnmatch pattern (default is \"*\")",
      "   -s TEXT : append TEXT to fnmatch pattern (default is \"*\")",
      "   -f TEXT : set your favourite text, which is added to the search when you type ';'",
      "   -P      : equivalent to -p \"\"",
      "   -S      : equivalent to -s \"\"",
      "   -Q      : disable exit (and fail) on two consecutive q characters",
      "   -h      : output this help message",
      0
   };

void usage()
{

   for (int i=0; usage_message[i]; i+=1)
      fprintf(stdout,"%s\n", usage_message[i]);
}

/* **********************************************************************
 * Start/end curses.
 */

void curses_start()
{
   FILE *fd;

   // Open /dev/tty directly, so that standard input and/or standard output can
   // be redirected.
   if ( !(fd = fopen("/dev/tty", "r+")) )
      { fprintf(stderr, "failed to open /dev/tty\n"); exit(1); }

   set_term(newterm(NULL, fd, fd));
   cbreak(); 
   noecho();
   clear();
   raw();
   keypad(stdscr, TRUE);
   meta(stdscr, TRUE);
}

void curses_end()
{
   clrtoeol();
   refresh();
   endwin();
}

/* **********************************************************************
 * Utilities.
 */

void die(int err)
{
   curses_end();
   exit(err);
}

void fail(char *msg)
{
   curses_end();
   fprintf(stderr, "%s\n", msg);
   exit(1);
}

void *non_null(void *ptr)
{
   if ( !ptr )
   {
      curses_end();
      fprintf(stderr, "malloc failed\n");
      exit(1);
   }
   return ptr;
}

/* **********************************************************************
 * Globals.
 */

static char *prompt = "filter: ";
static int prompt_len = 0;

static char *prefix = "*";
static char *suffix = "*";
static int qq_quits = 1;
static int standard_input = 1; // tpick default =0
static char **cargv = 0;
static int cargc = 0;
static char *favourite = 0;
char *match; // boolean really
int matchCount = 0;

static int linec;
static char **lineTxt;

static WINDOW *prompt_win = 0;
static WINDOW *search_win = 0;
static WINDOW *main_win = 0;

/* **********************************************************************
 * Exit keys.
 * Exit on <ESCAPE>, <CONTROL-C> or, normally, two consecutive 'q's.
 */

void quit(const int c, const char *kn)
{
   static int qcnt = 0;

   if ( qq_quits ) {
      if ( c == 'q' ) { if ( qcnt ) die(1); else qcnt += 1; }
      else qcnt = 0;
   }

   if (c == 27 /* escape */ || strcmp(kn,"^C") == 0 )
      die(1);
      
}

/* **********************************************************************
 * Slurp standard input into lineTxt/linec.
 */

#define MAX_LINE 4096

void add_thing(char *buf)
{
   lineTxt = (char **) non_null(realloc(lineTxt,(linec+1)*sizeof(char *)));
   lineTxt[linec] = (char *) non_null(strdup(buf));
   linec += 1;
}

void read_standard_input_lines()
{
   char buf[MAX_LINE];

   linec = 0; lineTxt = 0;
   while ( fgets(buf,MAX_LINE,stdin) )
   {
      char *newline = strchr(buf,'\n');
      if ( newline ) newline[0] = 0;
      add_thing(buf);
   }
}

const char whitespace[] = " \t\b\v\r\n";

void read_standard_input_words()
{
   char buf[MAX_LINE];

   linec = 0; lineTxt = 0;
   while ( fgets(buf,MAX_LINE,stdin) )
   {
      char *tok = strtok(buf,whitespace);
      while ( tok )
      {
         add_thing(tok);
         tok = strtok(NULL,whitespace);
      }
   }
}

/* **********************************************************************
 * Main.
 */

void display(int c, char *kn);
void re_display();

void updateResults(char *needle);

int main(int original_argc, char *original_argv[])
{

   prompt_len = strlen(prompt);
   linec = original_argc; 
   lineTxt = original_argv;

   int opt;
   while ( (opt = getopt(linec, lineTxt, "Qp:s:PSlwhf:")) != -1 )
   {
      switch ( opt )
      {
         case 'Q': qq_quits = 0; break;
         case 'p': prefix = optarg; break;
         case 's': suffix = optarg; break;
         case 'P': prefix = ""; break;
         case 'S': suffix = ""; break;
         case 'l': standard_input = 0; break;
         case 'w': standard_input = 2; break;
         case 'f': favourite = optarg; break;
         case 'h':usage(0); exit(0); break;
         default: usage(1); exit(1);
      }
   }

   lineTxt += optind;
   linec -= optind;

   if ( standard_input && linec )
      { cargv = lineTxt; cargc = linec; }

   if ( original_argc == 1 && isatty(STDIN_FILENO)) // abort if no cmd line and no pipe
      { usage(0);fprintf(stderr, "nothing from which to pick\n"); exit(1); }

   if ( standard_input == 1 ) read_standard_input_lines();
   if ( standard_input == 2 ) read_standard_input_words();

   match = malloc( linec*sizeof(char ) );

   signal(SIGINT,  die);
   signal(SIGQUIT, die);
   signal(SIGTERM, die);
  // signal(SIGWINCH, re_display); // default resize handling seems to be best

   ESCDELAY = 10; // quick quit on escape key

   curses_start();
   main_win = newwin(LINES-1,COLS,1,0);
   prompt_win = newwin(1,prompt_len,0,0);
   search_win = newwin(1,COLS-prompt_len,0,prompt_len);
   waddstr(prompt_win,prompt);

   re_display();
   updateResults(""); // cache the results to start off with. TODO should respect command line args

   while ( 1 ) {
      int c = getch();
      char *kn = (char *) keyname(c);
      quit(c,kn);
      display(c,kn);
   }
}

/* **********************************************************************
 * Pattern matching.
 */

static char *fn_pattern = 0;
static int fn_flag = 0;

void fn_match_init(char *needle)
{
   int i, len = strlen(needle);

   // Smartcase.
   fn_flag = FNM_CASEFOLD;
   for (i=0; i<len && fn_flag; i+=1)
      if ( isupper(needle[i]) )
         fn_flag = 0;

   fn_pattern = (char *) non_null(realloc(fn_pattern,strlen(prefix)+len+strlen(suffix)+1));
   sprintf(fn_pattern, "%s%s%s",prefix,needle,suffix);
}

int fn_match(char *haystack)
   { return !fnmatch(fn_pattern,haystack,fn_flag); }

/* **********************************************************************
 * Display and selection handling.
 */

void handle_selection(char *selection) // TODO command doesn't work, but maybe this feature isn't worth maintaining and should be removed?
{
   if ( cargv )
   {
      char **command = (char **) non_null(malloc((cargc+1)*sizeof(char *)));
      memcpy(command,cargv,(cargc)*sizeof(char *));
      command[cargc] = selection;
      execvp(command[0],command);
      fprintf(stderr, "execvp failed: %s\n", command[0]);
      exit(1);
   }
   else
      printf("%s\n", selection);
}

void re_display()  { display(0,0);  } // causes refresh of screen with no button press

void dbg(int i) // for interactive debugging keep track of single int on screen realtime
{
char tmp[32];   
wmove(prompt_win, 0,0);
sprintf(tmp, "%d", i);
waddstr(prompt_win,tmp);
wclrtoeol(prompt_win);
}

void updateResults(char *needle)
{
// TODO rather than just caching the results in a sparse list we could rewrite the list which would be faster still but this works ok
fn_match_init(needle);
matchCount = 0;
for (int i = 0; i < linec; i += 1) // linec/lineTxt are actually the lines/etc passed into tpick
   if (fn_match(lineTxt[i]))
      {
      match[i] = 1;
      matchCount++;
      }
   else
      {
      match[i] = 0;
      }
}

void display(int c, char *key)  // c= character just pressed, key = in text
{
   static char *selection = 0;
   static char *search = 0;
   static int search_len = 0;
   static int current = 0; // current selection (indexed count into filtered list)
//   static int bottom = -1;
 //  static int end=0;
   int change = 0;


   if ( c == '\n' ) { // end on enter
      curses_end();
      if ( ! selection )
         exit(1);
      handle_selection(selection);
      exit(0);
   }

   if ( !search )
      search = (char *) non_null(strdup(""));

   if ( c == ';' && favourite )
   {
      char *ptr;
      for (ptr=favourite; ptr[0]; ptr+=1)
         display(ptr[0],0);
      return;
   }

   if ( key && strcmp(key,"KEY_DOWN") == 0 )
      { current += 1; c = 0; }

   if ( key && strcmp(key,"KEY_UP") == 0 )
      { current -= 1; c = 0; }

   if ( key && strcmp(key,"KEY_NPAGE") == 0 )
      { current += LINES / 2; c = 0; }

   if ( key && strcmp(key,"KEY_PPAGE") == 0 )
      { current -= LINES / 2; c = 0; }

    if ( key && strcmp(key,"KEY_HOME") == 0 )
      { current = 0; c = 0; }   

if ( key && strcmp(key,"KEY_END") == 0 )
      { current = matchCount; c = 0; }   


   if ( c == ' ' )
      { c = '*'; key = (char *) keyname(c); }

   if ( isalnum(c) || ispunct(c) )
      { change = 1; search[search_len] = c; }

   if ( c == KEY_BACKSPACE && 0 < search_len )
      change = -1;
   
   if
    ( change ) { // any change to search string
      search_len += change;
      search = (char *) non_null(realloc(search,search_len+1)); // todo seems pointless to maloc this
      search[search_len] = 0;
      wclear(search_win);
      waddstr(search_win,search);
      current = 0; // reset to top of results

      updateResults(search);
    //  bottom = linec; // where is the end of the results? we will have to calculate
    //  end = bottom;
      wclear(main_win); // remove displayed results from screen, as the redrawed list will be shorter
     /*
      fn_match_init(search);

      // TODO rather than just caching the results in a sparse list we could rewrite the list which would be faster still but this works ok
      matchCount = 0;
      for (i=0; i<linec; i+=1) // linec/lineTxt are actually the lines/etc passed into tpick
         if ( fn_match(lineTxt[i]) ) 
         {
            match[i] = 1;
            matchCount++;
         }
            else
            {
               match[i] = 0;
            }
    */        
   }

   wmove(main_win, 0,0); // will leave filter untouched
   curs_set(0); // turn off cursor before drawing
   selection = 0;
  // int top;
 // fn_match_init(search);
   
  int y = 0; // position we draw each line to on the screen
 
  current = middle(0, current, matchCount-1); // acceptable range 0...filtered(linec)-1
  int top = middle(0, (current-LINES/2), matchCount-LINES+1);//linec-LINES+1); // don't scroll any further than what is needed to see the bottom

   for (int i=0; i<linec; i+=1) // linec/lineTxt are actually the lines/etc passed into tpick
      if ( match[i] ) // now using cached results, faster plus we know where the end is
      {  
         y += 1; // we can't use i as y coord because we skip non-matches. So j is our y coordinate

   		if (y < top) // we updated y already but we have not gotten to top of the window yet
   			continue;

         if ( y >= (LINES+top) ) // bottom of the screen
         	break;
            
         //if ( selection == 0 )
         //   selection = lineTxt[i];
         if ( y-current == 1 ) { wattron(main_win, A_REVERSE); selection = lineTxt[i];}
         waddstr(main_win,lineTxt[i]);
         if ( y-current == 1 ) wattroff(main_win, A_REVERSE);
         wclrtoeol(main_win); // clear to end of line
       //  wmove(main_win,j-offset,0); always at the top option
       wmove(main_win, y-top,0); 
      }
   /*
   if (i == linec)
      {
      if (y >= (LINES+top))
         {
         wclear(main_win);
         end = y-1;
         goto redo;
         }
      }

   bottom=y;
   */
   
//   // Ensure we don't scroll off the bottom of the list.
//   if ( offset && j <= offset )
//   {
//      offset = j ? j-1 : 0;
//      re_display();
//      return;
//   }

   refresh();
   wrefresh(main_win);
   wrefresh(prompt_win);
   wrefresh(search_win);
   wmove(search_win,0,search_len);
   curs_set(1);
}


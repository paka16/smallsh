#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>  // for waitpid()
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];
size_t wordsplit(char const *line);
char * expand(char const *word);

/* GLOBAL VARIABLES? */
int amp_flag = -1;
char dty[30];
int right_flag = -1;
int left_flag = -1;
int append_flag = -1;
/* doing *c_arg -> makes c_arg in read-only mem, 
 * but i want to write to it. so don't do *. */
char *c_arg[MAX_WORDS];


int nbic = -1;  // non-built-in command
pid_t bg_pid = 0; 
int process_status = 0;
pid_t processid;
pid_t child_pid;

int interactive = 0;

int signal_number = 0;

int exiting = 0;
int signaling = 0;
int this_pwd = 0;

/* SIGNAL HANDLING */
void handle_SIGINT(int sig) {}

/* * * * * * * * * */


int main(int argc, char *argv[])
{ 
  /* SETTING THE input VARIABLE TO READ FROM STDIN. */
  FILE *input = stdin;
  char *input_fn = "(stdin)";  /* DEFAULTING THE INPUT'S FILENAME TO BE 'stdin' - TRACKING PURPOSES. */ 

  /* IF 2 ARGUMENTS -> READ FROM FILE? */
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");  /* READ ONLY AND HANDLES THE CLOEXEC */
    /* ERROR - THERE'S NO INPUT. */
    if (!input) {
      // printf("DEBUGGING: There's no input - LINE 30.");
      err(1, "%s", input_fn);
    }
  } /* IF MORE THAN 2, ERROR */ 
  else if (argc > 2) {
    errx(1, "too many arguments");
  }

  char *line = NULL;
  size_t n = 0;

  errno = 0; // set errno here?
             // // init pid here?
  processid = getpid();

  struct sigaction c_oldact = {0}, z_oldact = {0}, SIGINT_action = {0}, SIGTSTP_action = {0}; 

  for (;;) {
    // TRY SIGNAL HANDLING HERE?
/* PROMPT LABEL - COMMENTED OUT FOR NOW. */
prompt:;
    /* TODO: Manage background processes */  // REPORTING ABOUT BG PROCESSES - REPORT THE STATUS.
       // reporting with waitpid() tho? 
       // MIGHT HAVE TO RUN MULTIPLE TIMES (THE MGMT PROCESS) TO GET ALL THE INFORMATION
    // check for any un-waited-for bg process in the same process groupt id as smallsh? 

    int status_check;
    pid_t mgmt_pid = waitpid(-1, &status_check, WNOHANG | WUNTRACED); 

    // printf("DEBUGGING - mgmt_pid = %d\n", mgmt_pid);

    // process_status = WEXITSTATUS(status_check);

   while (mgmt_pid > 0) {
      if (WIFEXITED(status_check) != 0 && mgmt_pid != 0) {
        process_status = WEXITSTATUS(status_check);
        // exiting_prompt:;  - 2/6/24 - i think this is just for waitpid / management process not purely exit.
        // DO I NEED THIS HERE?????? - 2/7 
        // printf("DEBUGGING - here?\n");
        
        fprintf(stderr,"Child process %jd done. Exit status %d.\n", (intmax_t) mgmt_pid, process_status); 
        exit(0); 
      }
      else if (WIFSIGNALED(status_check) != 0 && mgmt_pid != 0) {
         process_status += 128;
         processid = mgmt_pid;
         // printf("DEBUGGING - in signal\n");
         // process_status = WEXITSTATUS(status_check);
         fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) mgmt_pid, WTERMSIG(status_check));
         
         exit(0); // exit here? 2/7/24
      } 
      else if (WIFSTOPPED(status_check)) {
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) mgmt_pid);
        bg_pid = mgmt_pid;

        kill(mgmt_pid, SIGCONT);
      }
      mgmt_pid = waitpid(-1, &status_check, WNOHANG | WUNTRACED);   
   }
    /* TODO: prompt */
    processid = getpid();
    if (input == stdin) { /* IF INTERACTIVE MODE. */
      // printf("DEBUGGING - INTERACTIVE MODE\n");
      // if interactive mode is interrupted => print new line. 
      interactive = 1;
      char* prompt_format = getenv("PS1");
      char *exp_prompt = expand(prompt_format);
      fprintf(stderr, "%s", exp_prompt);

      /* IF INTERRUPTED BY A SIGNAL 
       * PRINT A NEW LINE
       * PRINT COMMAND PROMPT AND CHECK FOR BF PROCESSES
       * AND READ NEW LINE 
       * RESET ERRNO */
  //    if (errno == EINTR) {    // 2/6/24
  //       // printf("DEBUGGING - errno = EINTER\n");
  //       errno = 0;  // reset errno 
  //    }
  //    clearerr(input);
     
    } else {
      interactive = 0;
    }

    /* SIGNAL HANDLING? */
    if (interactive == 1) {
    
      // ignore SIGTSTP
      // ignore SIGINT by SIG_IGN wexcept when reading a line of input in step 1
      // 

      // register the handlers
      SIGINT_action.sa_handler = SIG_IGN;  // needs handler before getline to get the -1 error return value.
      SIGTSTP_action.sa_handler = SIG_IGN;

       
      // blocking all catchable signals while handler is running. 
      sigfillset(&SIGINT_action.sa_mask);
      sigfillset(&SIGTSTP_action.sa_mask);

      // no flags set???
      SIGINT_action.sa_flags = 0;
      SIGTSTP_action.sa_flags = 0;

      // save the original disposition into oldact. 
      // putting in the oldact struts to write in the dispositions.
      // writing into oldacts
      sigaction(SIGTSTP, &SIGTSTP_action, &z_oldact);
      sigaction(SIGINT, &SIGINT_action, &c_oldact);

     

    }
    /* * * * * * * * * * * * * * * * * */

    /* READING THE INPUT.
     * WE'RE PASSING IN A POINTER TO THE ADDRESS OF THE LINE.
     * THEN WE'RE PASSING IN n WHICH WILL TRACK THE SIZE OF THE BUFFER
     * */
     // reset it after getline? 
    if (errno == EINTR) {    // 2/6/24
      // printf("DEBUGGING - errno = EINTER\n");
        errno = 0;  // reset errno 
    }
    clearerr(input); 

    
  

    ssize_t line_len = getline(&line, &n, input);
   // printf("line_len = %ld\n", line_len);
 
    /* ERROR HANDLING: IF line_len IS LESS THAN 0 */
    if (line_len < 0) {
    //  printf("DEBUGGING: line_len < 0 - LINE 56.");
     // printf("DEBUGGING - errno = %d\n", errno);
        if (EOF) {
          exit(process_status);
        }
        if (interactive == 1) {
       //  printf("interactive?\n");
         SIGINT_action.sa_handler = handle_SIGINT;
         sigfillset(&SIGINT_action.sa_mask);
        SIGINT_action.sa_flags = 0;
        sigaction(SIGINT, &SIGINT_action, &c_oldact); 

        if (errno == EINTR) {    // 2/6/24
          // printf("DEBUGGING - errno = EINTER\n");
          errno = 0;  // reset errno 
        }
        clearerr(input); 
        goto prompt;
      }
      if (EOF) {
       //  printf("eof?\n");
        exit(process_status);
      }
      else {
        err(1, "%s", input_fn); 
      }
    }

 //   SIGINT_action.sa_handler = SIG_IGN;
 //   sigfillset(&SIGINT_action.sa_mask);
    // SIGINT_action.sa_flags = 0;
    // sigaction(SIGINT, &SIGINT_action, NULL);  // passes 20 and 21
    
    size_t nwords = wordsplit(line);
    int cd_flip = -1; 
    
    for (size_t i = 0; i < nwords; ++i) {
     // fprintf(stderr, "Word %zu: %s\n", i, words[i]);
      // printf("words[i]: %s", words[i]);
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
      // fprintf(stderr, "Expanded Word %zu: %s\n", i, words[i]);


      // printf("words[i]: %s", words[i]);
      if (strcmp(words[0], "exit") == 0) {
        /* IF AN EXIT STATUS EXITS 
         * NO EXIT PRINT STATEMENTS NEEDED !!!!! - 2/7*/
        
        if (words[1]) {
          process_status = atoi(words[1]);
          // printf("DEBUGGING - process_status = %d\n", process_status);
         // fprintf(stderr, "Child process %d done. Exit status %d.\n", processid, process_status);
         exit(process_status);
        }
      
        // else, it's just exit 0?
       // fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t)processid, process_status); // status?
        // goto exit label?
       exit(process_status); 
       
       // goto exiting_prompt;
      }
      else if (strcmp(words[i], "cd") == 0) {      
        cd_flip = 0;
        // printf("DEBUGGING -  word == cd\n");
        if (i == (nwords - 1)) {
            sprintf(dty, "%s", getenv("HOME"));
            // printf("DEBUGGING - dty = %s\n", dty);
            goto chdir_l;
        }         
        
        continue; 
        // have a variable to note the cd, like cd == 0 and ch dir to the next word?
        // printf("word[i]: %s\n", words[i]);
      }
      else if (cd_flip == 0) {
        // printf("DEBUGGIN IN CD FLIP\n");
        sprintf(dty, "%s", words[i]);
        cd_flip = -1;
        // printf("DEBUGGING: changing directory to %s.\n", words[i]);
        /* CHDIR ERROR HANDLING 
         * RETURNS 0 ON SUCCESS, -1 ON FAILURE.
         * ERRNO IS ALSO SET - CHECK ERRNO.
         * */
        chdir_l:;

       

          // printf("DEBUGGING:  cd_flip: %d\n", cd_flip);
          // printf("DEBUGGING: words[i] = %s\n", words[i]);
          
          if (chdir(dty) != 0) {
            // printf("DEBUGGING - CAN'T CHANGE directory\n");
            // printf("errno: %d\n", errno);
            if (errno == EACCES) printf("errno == EACCES -> permission denied.\n");
            err(-1, "%s", words[i]);
          } 
          /* CHDIR() SUCCESSFUL */
          // might have to recheck this cause even if not chdir, we might go in here.
         // else {
            // AFTER CHDIR, GO BACK TO PROMPT?
            // printf("DEBUGGING: CHDIR() SUCCESSFUL. \n"); 
          // }
        } 
      /* NON-BUILT IN COMMANDS?  = FORKING HERE. */

      else { 
        // printf("DEBUGGING - non-built-in command.\n");
        nbic = 0; 
      }
     //  printf("DEBUGGING - HERE?\n");

    } // end of words for loop. 

    if (nbic == 0) {
 
      pid_t spawnpid = -5;
      int childStatus;
  
      spawnpid = fork();
      switch (spawnpid) {
        case -1:
          perror("DEBUGGING - fork() failed!\n");
          exit(1);
          // break;  // keep this break 2/6/24
        case 0:
          // parsing here! 
          // printf("DEBUGGING - nwords = %ld\n", nwords);
          //
          if (interactive == 1) { 
            sigaction(SIGTSTP, &z_oldact, NULL);
            sigaction(SIGINT, &c_oldact, NULL);

             
           

          } 

                    
          for (int parse_ind = 0; parse_ind < nwords; parse_ind++) {
            // printf("DEBUGGING -  parse_ind: %d\n", parse_ind);
            // printf("DEBUGGING: words[parse_ind] = %s, parse_ind = %d\n", words[parse_ind], parse_ind);
            
            if (strcmp(words[parse_ind], ">>") == 0) {
              // printf("DEBUGGING -  OPEN/APPENDING.\n");

              char* redirect = words[parse_ind + 1];
              int new_file = open(redirect, O_CREAT |O_RDWR | O_APPEND | O_CLOEXEC, 0777);
              if (new_file < 0) printf("DEBUGGING - COULDN'T OPEN %s\n", words[parse_ind]);
              if (dup2(new_file, 1) == -1) printf("DEBUGGING - dup2 error\n"); 
              
              // update parse_int 
              parse_ind++;
              
            }  

            else if (strcmp(words[parse_ind], ">") == 0) {
              // printf("DEBUGGING - OPEN/WRITING.\n");
              char* redirect = words[parse_ind + 1];
              int new_file = open(redirect, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0777);
              if (new_file < 0) printf("DEBUGGING - COULDN'T OPEN %s\n", words[parse_ind]);
              if (dup2(new_file, 1) == -1) printf("DEBUGGING - dup2 error\n"); 
              
              // update parse_int 
              parse_ind++;
            } 
            else if (strcmp(words[parse_ind], "<") == 0) {
              // printf("DEBUGGING - OPEN/READING.\n");
              char* redirect = words[parse_ind + 1];
              int new_file = open(redirect, O_RDONLY);
              if (new_file < 0) printf("DEBUGGING - COULDN'T OPEN %s\n", words[parse_ind]);
             // printf("DEBUGGING - new_file opened and reading for reading.\n");
              if (dup2(new_file, 0) == -1) printf("DEBUGGING - dup2 error\n");
              close(new_file);

              parse_ind++; 
            } 
       
            else {
              // printf("DEBUGGING - this should hold other args.\n");
              // printf("DEBUGGING - adding words[parse_ind]: %s\n", words[parse_ind]);
              // c_arg[parse_ind] = strdup(words[parse_ind]);  // - only gets the first world. 
              char* token = strdup(words[parse_ind]);
              // printf("DEBUGGING - token = %s\n", token);
              c_arg[parse_ind] = token;
              // printf("DEBUGGING - buff check - c_arg = %s\n", c_arg[parse_ind]); 
            }
          } // done with parsing. 
          // printf("DEBUGGING - done with parsing \n");
           /* SIGNAL HANDLING? */
        
 
          
 
          execvp(c_arg[0], c_arg); 
          perror("DEBUGGING: ERROR ON EXECVP.\n");
          exit(2);
          // break;  // keep this break 2/6/24
                      
        default: 
          // printf("DEBUGGING - parent: %d\n", getpid()); 
          // printf("Parent (%d): child (%d) terminated. Exiting.\n", getpid(), spawnpid);
          // NON-BLOCKING WAIT ON FOREGROUND CHILD PROCESS.
          // process_status = WEXITSTATUS(childStatus); 
          if (nbic == 0 && amp_flag == -1) {
            // printf("DEBUGGING - WAITING IF\n");
            int w = waitpid(spawnpid, &childStatus, WUNTRACED);
            if (w == -1) {
              err(1, "WAITING FAILED.\n");
              exit(EXIT_FAILURE);
            } 
            // $? -> exit status of the waited-for command
           // process_status = WEXITSTATUS(childStatus); 
            if (WIFEXITED(childStatus) != 0) {
              process_status = WEXITSTATUS(childStatus);
            }
            if (WIFSIGNALED(childStatus) != 0) {
              // printf("DEBUGGING - WIFSIGNALED\n");
              signaling = 1;
              process_status = 128 + WTERMSIG(childStatus); 
            }
          
            else if (WIFSTOPPED(childStatus) != 0) {
              // printf("DEBUGGING -  stopped\n");
              // SEND SIGCONT?????
              // child_pid = getpid();
              fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) spawnpid);
              bg_pid = spawnpid;
              processid = getpid();  // should be the child process status. 

              kill(spawnpid, SIGCONT); 
                                     

             
            } 
          }
           else {  // NOT WAITING  --- STILL NEEDS WAITPID()????
              // child process runs in the background
              // bg_pid = spawnpid
             // printf("DEBUGGING - in waiting else\n");
            
             // if (WIFEXITED(process_status)) {
             //   process_status = WEXITSTATUS(childStatus);
             //  } 
              bg_pid = spawnpid;
              processid = spawnpid;
              
            }
         // break;  // do we need this? 2/6/24
      } // end of switch (spawnpid)
  } // end of if nbic case
      
    /* RESET EVERYTHING? */
    // printf("DEBUGG - TIME TO RESET?\n");
    right_flag = -1;
    nbic = -1;
    amp_flag = -1;
    left_flag = -1;
    append_flag = -1; 
    
    // printf("DEBUGGING - rightflag = %d\n", right_flag);

  }
}

char *words[MAX_WORDS] = {0};

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 *
 * MAKING TOKENS?
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;

  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */ 

  for (; *c;) { 
    /* IF INDEX IS OUT OF BOUNDS, BREAK. */
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    // have a flag for &? 
    // if set, relize we need to have it as a background operator?
    if (*c == '&') {
      // printf("DEBUGGING: found an &.\n");
      // is it the last character though? 
      amp_flag = 0;
      break;  // because don't include it into our word list? 
    }
    // this is for writing out
    // so technically, the next token should be the stdout. 
    // do this in main though because expansion should happen first? 
    if (*c == '>') {
      // printf("DEBUGGING - found >\n");
      right_flag = 0; 
    }
    if (*c == '<') {
      left_flag = 0;
    }
    if (*c == '>' && right_flag == 0) {
      append_flag = 0;
    }
    if (*c == '/') {
      this_pwd = 1;
    }
 
    for (;*c && !isspace(*c); ++c) {
      // printf("DEBUGGING: *c = %c\n", *c); 
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      /* IF realloc FAILS, ERROR CATCHING. */
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  return wind;
}


/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 * LOOKING FOR DOLLAR SIGN.
 */
char
param_scan(char const *word, char const **start, char const **end)
{
  static char const *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = 0;
  *end = 0;
  for (char const *s = word; *s && !ret; ++s) {
    s = strchr(s, '$');
    if (!s) break;
    switch (s[1]) {
    case '$':
    case '!':
    case '?':
      ret = s[1];
      *start = s;
      *end = s + 2;
      break;
    case '{':;
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = s[1];
        *start = s;
        *end = e + 1;
      }
      break;
    }
  }
  prev = *end;
  return ret;
}


/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0; 

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';


  return base;
}


char *
slice_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *param_base = 0; 

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = param_base;
    param_base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *param_base *(base_len + n + 1);
  void *tmp = realloc(param_base, newsize);
  if (!tmp) err(1, "realloc");
  param_base = tmp;
  memcpy(param_base + base_len, start, n);
  base_len += n;
  param_base[base_len] = '\0';

  // printf("DEBUGGING - param_base: %s\n", param_base);

  return param_base;
}


/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char const *start, *end;
  char c = param_scan(pos, &start, &end);
  build_str(NULL, NULL);  // clears out build_str's internal memory. 
  build_str(pos, start);
  
  // printf("DEBUGGING: word: %s\n", word);
  
  

  while (c) { 
    // printf(" c = %c\n", c);
      
    if (c == '!') {
      // build_str("<BGPID>", NULL);
      /* IF NO BG PROCESSES */
      char bg_buf[18];
      sprintf(bg_buf, "%d", bg_pid);
      build_str(bg_buf, NULL);
    }
    /* GETPID */
    else if (c == '$') {
      // printf("DEBUGGING: GETTING PID: %d - LINE 195.\n", getpid());   
      char pid_buf[18]; /* FREE LATER ON? */
      // this is statically allocated so no need to free, does it by itself. 
      sprintf(pid_buf, "%d", processid); 
      build_str((char*) &pid_buf, NULL);
    }
    else if (c == '?') {
      /* DEFAULT = 0 */
      char pstatus[18]; 
      sprintf(pstatus, "%d", process_status);
      build_str(pstatus, NULL);
    }
    else if (c == '{') {
       slice_str(NULL, NULL);  // clear out the previous
       // printf("here!\n");
       build_str(getenv(slice_str(start + 2, end - 1)), NULL);
      
     
    } // end of else if {

   
  
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  return build_str(start, NULL);
}




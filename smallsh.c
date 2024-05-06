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
prompt:;

    int status_check;
    pid_t mgmt_pid = waitpid(-1, &status_check, WNOHANG | WUNTRACED); 

   while (mgmt_pid > 0) {
      if (WIFEXITED(status_check) != 0 && mgmt_pid != 0) {
        process_status = WEXITSTATUS(status_check);
        
        fprintf(stderr,"Child process %jd done. Exit status %d.\n", (intmax_t) mgmt_pid, process_status); 
        exit(0); 
      }
      else if (WIFSIGNALED(status_check) != 0 && mgmt_pid != 0) {
         process_status += 128;
         processid = mgmt_pid;
         fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) mgmt_pid, WTERMSIG(status_check));
         exit(0);
      } 
      else if (WIFSTOPPED(status_check)) {
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) mgmt_pid);
        bg_pid = mgmt_pid;
        kill(mgmt_pid, SIGCONT);
      }
      mgmt_pid = waitpid(-1, &status_check, WNOHANG | WUNTRACED);   
   }
    processid = getpid();
    if (input == stdin) { 
      interactive = 1;
      char* prompt_format = getenv("PS1");
      char *exp_prompt = expand(prompt_format);
      fprintf(stderr, "%s", exp_prompt);
     
    } else {
      interactive = 0;
    }

    if (interactive == 1) {
      SIGINT_action.sa_handler = SIG_IGN;  // needs handler before getline to get the -1 error return value.
      SIGTSTP_action.sa_handler = SIG_IGN;

      sigfillset(&SIGINT_action.sa_mask);
      sigfillset(&SIGTSTP_action.sa_mask);

      SIGINT_action.sa_flags = 0;
      SIGTSTP_action.sa_flags = 0;

      sigaction(SIGTSTP, &SIGTSTP_action, &z_oldact);
      sigaction(SIGINT, &SIGINT_action, &c_oldact);
    }
    /* * * * * * * * * * * * * * * * * */

    if (errno == EINTR) {   
        errno = 0;  // reset errno 
    }
    clearerr(input); 

    ssize_t line_len = getline(&line, &n, input);
    if (line_len < 0) {
    
        if (EOF) {
          exit(process_status);
        }
        if (interactive == 1) {
     
         SIGINT_action.sa_handler = handle_SIGINT;
         sigfillset(&SIGINT_action.sa_mask);
        SIGINT_action.sa_flags = 0;
        sigaction(SIGINT, &SIGINT_action, &c_oldact); 

        if (errno == EINTR) {   
      
          errno = 0;  
        }
        clearerr(input); 
        goto prompt;
      }
      if (EOF) {
     
        exit(process_status);
      }
      else {
        err(1, "%s", input_fn); 
      }
    }

    
    size_t nwords = wordsplit(line);
    int cd_flip = -1; 
    
    for (size_t i = 0; i < nwords; ++i) {
     
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
    
      if (strcmp(words[0], "exit") == 0) {
   
        if (words[1]) {
          process_status = atoi(words[1]);
         exit(process_status);
        }
      
       exit(process_status); 
       
      }
      else if (strcmp(words[i], "cd") == 0) {      
        cd_flip = 0;
        if (i == (nwords - 1)) {
            sprintf(dty, "%s", getenv("HOME"));
            goto chdir_l;
        }         
        
        continue; 
      }
      else if (cd_flip == 0) {
       
        sprintf(dty, "%s", words[i]);
        cd_flip = -1;
        chdir_l:;

          if (chdir(dty) != 0) {
            if (errno == EACCES) printf("errno == EACCES -> permission denied.\n");
            err(-1, "%s", words[i]);
          } 
      else { 
        nbic = 0; 
      }
    }

    if (nbic == 0) {
 
      pid_t spawnpid = -5;
      int childStatus;
  
      spawnpid = fork();
      switch (spawnpid) {
        case -1:
          perror("DEBUGGING - fork() failed!\n");
          exit(1);
        case 0:
          if (interactive == 1) { 
            sigaction(SIGTSTP, &z_oldact, NULL);
            sigaction(SIGINT, &c_oldact, NULL);
          } 
  
          for (int parse_ind = 0; parse_ind < nwords; parse_ind++) {
            if (strcmp(words[parse_ind], ">>") == 0) {
              char* redirect = words[parse_ind + 1];
              int new_file = open(redirect, O_CREAT |O_RDWR | O_APPEND | O_CLOEXEC, 0777);
              if (new_file < 0) printf("DEBUGGING - COULDN'T OPEN %s\n", words[parse_ind]);
              if (dup2(new_file, 1) == -1) printf("DEBUGGING - dup2 error\n"); 
              
              parse_ind++;
              
            }  

            else if (strcmp(words[parse_ind], ">") == 0) {
              char* redirect = words[parse_ind + 1];
              int new_file = open(redirect, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0777);
              if (new_file < 0) printf("DEBUGGING - COULDN'T OPEN %s\n", words[parse_ind]);
              if (dup2(new_file, 1) == -1) printf("DEBUGGING - dup2 error\n"); 
              
              parse_ind++;
            } 
            else if (strcmp(words[parse_ind], "<") == 0) {
              char* redirect = words[parse_ind + 1];
              int new_file = open(redirect, O_RDONLY);
              if (new_file < 0) printf("DEBUGGING - COULDN'T OPEN %s\n", words[parse_ind]);
              if (dup2(new_file, 0) == -1) printf("DEBUGGING - dup2 error\n");
              close(new_file);
              parse_ind++; 
            } 
       
            else {
              char* token = strdup(words[parse_ind]);
              c_arg[parse_ind] = token;
            }
          } 
          execvp(c_arg[0], c_arg); 
          perror("DEBUGGING: ERROR ON EXECVP.\n");
          exit(2);
                      
        default: 
         
          if (nbic == 0 && amp_flag == -1) {
            int w = waitpid(spawnpid, &childStatus, WUNTRACED);
            if (w == -1) {
              err(1, "WAITING FAILED.\n");
              exit(EXIT_FAILURE);
            } 
            if (WIFEXITED(childStatus) != 0) {
              process_status = WEXITSTATUS(childStatus);
            }
            if (WIFSIGNALED(childStatus) != 0) {  
              signaling = 1;
              process_status = 128 + WTERMSIG(childStatus); 
            }          
            else if (WIFSTOPPED(childStatus) != 0) {
              fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) spawnpid);
              bg_pid = spawnpid;
              processid = getpid();  // should be the child process status. 
              kill(spawnpid, SIGCONT); 
                                                 
            } 
          }
           else {  
              bg_pid = spawnpid;
              processid = spawnpid;
            }
      } 
  } 
    right_flag = -1;
    nbic = -1;
    amp_flag = -1;
    left_flag = -1;
    append_flag = -1; 
  }
}

char *words[MAX_WORDS] = {0};

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 *
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
    if (*c == '&') {
      amp_flag = 0;
      break;  
    }
    if (*c == '>') {
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
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
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

  while (c) { 
      
    if (c == '!') {
      char bg_buf[18];
      sprintf(bg_buf, "%d", bg_pid);
      build_str(bg_buf, NULL);
    }
    /* GETPID */
    else if (c == '$') {
      char pid_buf[18]; /* FREE LATER ON? */
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
       build_str(getenv(slice_str(start + 2, end - 1)), NULL);
      
    } // end of else if {
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  return build_str(start, NULL);
}




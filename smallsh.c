// Macro Definitions
#define _POSIX_C_SOURCE 200809L 
#define _GNU_SOURCE 

// Header Files
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <err.h>
#include <signal.h>
#include <ctype.h>
#include <signal.h>
#include <stdint.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

// Global Variables
int backgroundFlag = 0;
int childStatus = 0;
int backgroundChild = 0;
//int foregroundChild = 0;
int result = 0;
int sourceFileDescriptor = 0;
int targetFileDescriptor = 0;
int appendFileDescriptor = 0;
char *inputFile = NULL;
char *outputFile = NULL;
char *foregroundPid = "0";
char *backgroundPid = "";
char *words[MAX_WORDS];
size_t nwords = 0;
pid_t pid;
char *charPid;
int intError = 1;
char *appendFile = NULL;

size_t wordsplit(char const *line);
char *expand(char const *word);

// Function for checking termination of background processes
void backgroundCheck() {
  while ((backgroundChild = waitpid(0, &childStatus, WUNTRACED | WNOHANG)) > 0) {
    if (WIFEXITED(childStatus)) {
      fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) backgroundChild, WEXITSTATUS(childStatus));
    }
    if (WIFSIGNALED(childStatus)) {
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) backgroundChild, WTERMSIG(childStatus));
      }
    if (WIFSTOPPED(childStatus)) {
      kill(backgroundChild, SIGCONT);
      fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) backgroundChild);
    }
  }
}
// Making signals do nothing.
void sig_handler(int sig) {}
// Function for Word Splitting
char *words[MAX_WORDS] = {0};
/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;
  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
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
 */
/*
char
param_scan(char const *word, char **start, char **end)
{
  static char *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = NULL;
  *end = NULL;
  char *s = strchr(word, '$');
  if (s) {
    char *c = strchr("$!?", s[1]);
    if (c) {
      ret = *c;
      *start = s;
      *end = s + 2;
    }
    else if (s[1] == '{') {
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = '{';
        *start = s;
        *end = e + 1;
      }
    }
  }
  prev = *end;
  return ret;
}
*/
/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
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
/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char const *start, *end;
  char c = param_scan(pos, &start, &end);
  build_str(NULL, NULL);
  build_str(pos, start);
  while (c) {
  if (c == '!') build_str(backgroundPid, NULL);
    else if (c == '$') build_str(charPid, NULL);
    else if (c == '?') build_str(foregroundPid, NULL);
    else if (c == '{') {
      //build_str("<Parameter: ", NULL);
      char *varenv = build_str(start + 2, end - 1);
      if (getenv(varenv) == NULL) varenv = "";
      build_str(NULL, NULL);
      build_str(getenv(varenv), NULL);
      //build_str(getenv(build_str(start + 2, end - 1)), NULL);
      //build_str(">", NULL);
    }
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  return build_str(start, NULL);
}

int main(int argc, char *argv[])
{
  struct sigaction SIGINT_default = {0};
  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = SIG_IGN;
  sigaction(SIGINT, &SIGINT_action, &SIGINT_default);
  
  struct sigaction SIGTSTP_default = {0};
  struct sigaction SIGTSTP_action = {0};
  SIGTSTP_action.sa_handler = SIG_IGN;
  sigaction(SIGTSTP, &SIGTSTP_action, &SIGTSTP_default);
  
  pid = getpid();
  charPid = malloc(10 * sizeof(int));
  sprintf(charPid, "%d", pid);
  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");
    if (!input) err(1, "%s", input_fn);
  } else if (argc > 2) {
    errx(1, "Error: Too many arguments");
  }  
  // Local variables
  char *line = NULL;
  char *PS1 = getenv("PS1");
  size_t n = 0;;
  // Infinite loop to run program.
  for (;;) {
prompt:;
    // Reset files and flags.
    backgroundFlag = 0;
    inputFile = NULL;
    outputFile = NULL;
    appendFile = NULL;
    for (size_t i = 0; i < nwords; ++i) {
      words[i] = NULL;
    }
    //errno = 0;
    // Manage Background processes
    backgroundCheck();
    // Printing command prompt
    if (PS1 == NULL) PS1 = "";
    fprintf(stderr, "%s", PS1);
    // Reading input
    
    SIGINT_action.sa_handler = sig_handler;
    sigaction(SIGINT, &SIGINT_action, NULL);
    errno = 0;
    if (input == stdin) {};
    ssize_t line_length = getline(&line, &n, input); // Reallocates line
    if (line_length < 0) {
      if (feof(stdin) != 0) {
        //foregroundPid = malloc(10 * sizeof(int));
        //sprintf(foregroundPid, "%d", 0);
        //exit((int) *foregroundPid);
        exit(0);
      }
      else if (errno == EINTR) {
        clearerr(stdin);
        errno = 0;
        fprintf(stderr, "\n");
        fflush(stdout);
        goto prompt;
      }
      else {
        //err(1, "%s", input_fn);
        exit(0);
      }
    }
    SIGINT_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_action, NULL);
    /* Word Splitting, Expanding */
    nwords = wordsplit(line);
    for (size_t i = 0; i < nwords; ++i) {
      //fprintf(stderr, "Word %zu: %s  -->  ", i, words[i]);
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
      //fprintf(stderr, "%s\n", words[i]);
    }

    /* Parsing */
    ssize_t i = 0;
    ssize_t count = nwords;
    count--;
    // Parsing
    if (words[0] != NULL && *words[nwords - 1] == '&') {
      words[nwords - 1] = NULL;
      backgroundFlag = 1;
    }

    /* Execution */
    // Check for empty input
    if (words[0] == NULL) {
      goto prompt;
    }
    // Check for valid exit and cd input
    if (words[0] != NULL) {
      // Check for valid exit input
      if (strcmp(words[0], "exit") == 0) {
        if (words[2] != NULL) {
          fprintf(stderr, "Error: Too many arguments.\n");
          foregroundPid = malloc(10 * sizeof(int)); // Storing child in foregroundPid
          sprintf(foregroundPid, "%d", intError);
          goto prompt;
        }
        if (words[1] != NULL) {
          if (!isdigit(*words[1])) {
            fprintf(stderr, "Error: Second argument is not an integer.\n");
            foregroundPid = malloc(10 * sizeof(int));
            sprintf(foregroundPid, "%d", intError);
            goto prompt;
          }
        }
        if (words[1] == NULL) {
          //fprintf(stderr, "");
          exit(0);
          //exit((int) *foregroundPid);
        }
        fprintf(stderr, "");
        exit(atoi(words[1]));
      }
      // Check for valid cd input
      if (strcmp(words[0], "cd") == 0) {
        if (words[2] != NULL) {
          fprintf(stderr, "Error: Too many arguments.\n");
          foregroundPid = malloc(10 * sizeof(int));
          sprintf(foregroundPid, "%d", intError);
          goto prompt;
        }
        if (words[1] == NULL) {
          words[1] = getenv("HOME");
        }
        if (chdir(words[1]) == -1) {
          fprintf(stderr, "Error: No directory found.\n");
          foregroundPid = malloc(10 * sizeof(int));
          sprintf(foregroundPid, "%d", intError);
        }
        words[1] = NULL;
        goto prompt;
      }
    }
    // Forking spawn and error checking streams.
    pid_t spawnPid = fork();
    //sigaction(SIGINT, &SIGINT_default, &SIGINT_action);
    //sigaction(SIGTSTP, &SIGTSTP_default, &SIGTSTP_action);
    switch(spawnPid) {
      // Forking error
      case -1:
        perror("fork()\n");
        exit(1);
        break;
      // Parsing and verifying input, output and append files and redirecting respective data.
      case 0:
        sigaction(SIGINT, &SIGINT_default, NULL);
        sigaction(SIGTSTP, &SIGTSTP_default, NULL);
        while (count > 0) {
          if (words[i] != NULL && strcmp(words[i], "<") == 0) {
            inputFile = words[i + 1];
            words[i + 1] = NULL;
            words[i] = NULL;
            if (inputFile != NULL) {
              sourceFileDescriptor = open(inputFile, O_RDONLY); 
              if (sourceFileDescriptor == -1) {
                perror("Error: Cannot open read file");
                exit(1);
              }
              result = dup2(sourceFileDescriptor, 0);
              if (result == -1) {
                perror("Error: Cannot duplicate read file");
                exit(2);
              }
            }
          }
          else if (words[i] != NULL && strcmp(words[i], ">") == 0) {
            outputFile = words[i + 1];
            words[i + 1] = NULL;
            words[i] = NULL;
            targetFileDescriptor = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (targetFileDescriptor == -1) {
              perror("Error: Cannot open write file");
              exit(1);
            }
            result = dup2(targetFileDescriptor, 1);
            if (result == -1) {
              perror("Error: Cannot duplicate write file");
              exit(2);
            }
          }
          else if (words[i] != NULL && strcmp(words[i], ">>") == 0) {
            appendFile = words[i + 1];
            words[i + 1] = NULL;
            words[i] = NULL;
            appendFileDescriptor = open(appendFile, O_WRONLY | O_CREAT | O_APPEND, 0777);
            if (appendFileDescriptor == -1) {
              perror("Error: Cannot open append file");
              exit(1);
            }
            result = dup2(appendFileDescriptor, 1);
            if (result == -1) {
              perror("Error: Cannot duplicate append file");
              exit(2);
            }
          }
          count--;
          i++;
        }
        execvp(words[0], words);
        if (inputFile != NULL) {
          close(sourceFileDescriptor);
        }
        if (outputFile != NULL) {
          close(targetFileDescriptor);
        }
        if (appendFile != NULL) {
          close(appendFileDescriptor);
        }
        exit(0);
        break;
      // If not a running background process, wait to finish
      default:
        if (backgroundFlag == 0) {
          spawnPid = waitpid(spawnPid, &childStatus, WUNTRACED);
          if (WIFSIGNALED(childStatus)) {
            foregroundPid = malloc(10 * sizeof(int));
            sprintf(foregroundPid, "%d", 128 + WTERMSIG(childStatus));
          } 
          else if (WIFSTOPPED(childStatus)) {
            kill(spawnPid, SIGCONT);
            backgroundPid = malloc(10 * sizeof(int)); // Storing child in foregroundPid
            sprintf(backgroundPid, "%d", spawnPid);
            fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) spawnPid);
          } 
          else {
            foregroundPid = malloc(10 * sizeof(int)); // Storing child PID in foregroundPid
            sprintf(foregroundPid, "%d", WEXITSTATUS(childStatus));
          }
        }
        else {
          backgroundPid = malloc(10 * sizeof(int));
          sprintf(backgroundPid, "%d", spawnPid); // Storing background PID in backgroundPid
        }
        goto prompt;
    }
  }
  return 0;
}


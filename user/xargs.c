#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h" // For MAXARG

#define MAX_LINE_LEN 512

int
main(int argc, char *argv[])
{
  char line[MAX_LINE_LEN];
  char *x_argv[MAXARG];
  int i;

  // 1. Prepare the argument list for the command to be executed.
  // The first argument to xargs (argv[0]) is "xargs" itself, so we skip it.
  // We copy the command and its initial arguments from xargs's arguments.
  for (i = 1; i < argc; i++) {
    x_argv[i-1] = argv[i];
  }

  // At this point, x_argv contains the base command, e.g., ["echo", "bye", NULL, ...].
  // The next available slot is at index (argc - 1).

  while(1) {
    int line_idx = 0;
    char c;

    // 2. Read one line from standard input, character by character.
    while (read(0, &c, 1) > 0) {
      if (c == '\n' || c == '\0') {
        break;
      }
      if (line_idx < MAX_LINE_LEN - 1) {
          line[line_idx++] = c;
      }
    }
    
    // If read returned 0, it means EOF (End of File), so we are done.
    if (line_idx == 0 && c != '\n') {
        break;
    }

    line[line_idx] = '\0'; // Null-terminate the line.

    // 3. Add the read line as the next argument.
    x_argv[argc - 1] = line;
    x_argv[argc] = 0; // The argument list must be null-terminated.
    
    // 4. Fork and execute the command.
    if (fork() == 0) {
      // Child process
      exec(x_argv[0], x_argv);
      // exec only returns if it fails.
      fprintf(2, "xargs: exec failed for command '%s'\n", x_argv[0]);
      exit(1);
    } else {
      // Parent process
      wait(0); // Wait for the child to finish.
    }
  }

  exit(0);
}

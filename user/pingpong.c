#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // p_to_c: parent to child pipe for "ping"
  // c_to_p: child to parent pipe for "pong"
  int p_to_c[2];
  int c_to_p[2];
  char buf[8]; // A small buffer is enough for one byte

  // 1. Create two pipes
  pipe(p_to_c);
  pipe(c_to_p);

  // 2. Fork a child process
  int pid = fork();

  if (pid < 0) {
    fprintf(2, "fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // --- This is the child process ---

    // The child reads from p_to_c and writes to c_to_p.
    // So, it must close the unused pipe ends.
    close(p_to_c[1]); // Close the write-end of parent-to-child pipe
    close(c_to_p[0]); // Close the read-end of child-to-parent pipe

    // Wait for the byte from the parent ("ping")
    if (read(p_to_c[0], buf, 1) != 1) {
      fprintf(2, "child: failed to read from parent\n");
      exit(1);
    }
    
    // Print the "ping" message
    printf("%d: received ping\n", getpid());

    // Send the byte back to the parent ("pong")
    if (write(c_to_p[1], buf, 1) != 1) {
      fprintf(2, "child: failed to write to parent\n");
      exit(1);
    }

    // Clean up: close the remaining used file descriptors
    close(p_to_c[0]);
    close(c_to_p[1]);

    exit(0);

  } else {
    // --- This is the parent process ---

    // The parent writes to p_to_c and reads from c_to_p.
    // So, it must close the unused pipe ends.
    close(p_to_c[0]); // Close the read-end of parent-to-child pipe
    close(c_to_p[1]); // Close the write-end of child-to-parent pipe

    // Send a byte to the child ("ping")
    if (write(p_to_c[1], "a", 1) != 1) { // We can send any byte
      fprintf(2, "parent: failed to write to child\n");
      exit(1);
    }

    // Wait for the byte from the child ("pong")
    if (read(c_to_p[0], buf, 1) != 1) {
      fprintf(2, "parent: failed to read from child\n");
      exit(1);
    }
    
    // Print the "pong" message
    printf("%d: received pong\n", getpid());
    
    // Clean up: close the remaining used file descriptors
    close(p_to_c[1]);
    close(c_to_p[0]);

    // Optional: wait for the child process to exit completely
    wait(0);
    
    exit(0);
  }
}

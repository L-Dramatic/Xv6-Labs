#include "kernel/types.h"
#include "user/user.h"

// The recursive part of the sieve
void sieve(int left_pipe_read_end) {
  int my_prime;
  int num;
  
  // 1. Read the first number from the left pipe. This is our prime.
  if (read(left_pipe_read_end, &my_prime, sizeof(int)) == 0) {
    // If the pipe is empty, this process has nothing to do.
    close(left_pipe_read_end);
    exit(0);
  }
  printf("prime %d\n", my_prime);

  // 2. Create a new pipe for the next stage of the sieve (the right pipe).
  int right_pipe[2];
  pipe(right_pipe);

  // 3. Fork a new process for the next stage.
  if (fork() == 0) {
    // --- Child Process (Next Stage) ---
    // It will read from the new (right) pipe.
    // So, close the write end of the right pipe and the old left pipe.
    close(right_pipe[1]);
    close(left_pipe_read_end);
    
    // Recursively call sieve with the read end of the new pipe.
    sieve(right_pipe[0]);

  } else {
    // --- Parent Process (Current Stage) ---
    // It will write to the new (right) pipe.
    // So, close the read end of the right pipe.
    close(right_pipe[0]);
    
    // 4. Read subsequent numbers from the left pipe and filter them.
    while (read(left_pipe_read_end, &num, sizeof(int)) > 0) {
      if (num % my_prime != 0) {
        // If not a multiple, pass it down to the right pipe.
        write(right_pipe[1], &num, sizeof(int));
      }
    }

    // 5. Clean up. Close both pipes and wait for the child to finish.
    close(left_pipe_read_end);
    close(right_pipe[1]);
    wait(0);
  }
  
  exit(0);
}

int
main(int argc, char *argv[])
{
  int p[2];
  pipe(p);

  if (fork() == 0) {
    // --- Child Process (The first sieve) ---
    // It only reads from the initial pipe. Close the write end.
    close(p[1]);
    sieve(p[0]);

  } else {
    // --- Parent Process (The number generator) ---
    // It only writes to the initial pipe. Close the read end.
    close(p[0]);

    // Feed numbers 2 to 35 into the pipe.
    for (int i = 2; i <= 35; i++) {
      if (write(p[1], &i, sizeof(int)) != sizeof(int)) {
        fprintf(2, "primes: write error\n");
        exit(1);
      }
    }
    
    // When done writing, close the write end. This sends an EOF.
    close(p[1]);
    
    // Wait for the entire pipeline to finish.
    wait(0);
  }

  exit(0);
}

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NCHILD 4
#define MSGMAX 128

void itoa(int n, char *s) {
  char tmp[16];
  int i = 0;
  do {
    tmp[i++] = n % 10 + '0';
    n /= 10;
  } while (n > 0);
  for (int j = 0; j < i; j++)
    s[j] = tmp[i - j - 1];
  s[i] = '\0';
}

int
main(int argc, char *argv[])
{
  int parent = getpid();
  int size = 4096;
  char *log = sbrk(size);

  printf("PARENT: pid=%d log=%p\n", parent, log);

  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      int mypid = getpid();
      printf("CHILD %d: pid=%d: mapping parent's log\n", i, mypid);

      char *child_log = (char*) map_shared_pages(log, parent, size);
      if ((uint64)child_log == (uint64)-1) {
        printf("CHILD %d: map_shared_pages failed\n", i);
        exit(1);
      }

      char msg[MSGMAX];
      strcpy(msg, "Hello from child ");
      itoa(i, msg + strlen(msg));

      int slot_size = size / NCHILD;
      char *slot = child_log + (i * slot_size);
      strcpy(slot, msg);

      printf("CHILD %d: wrote: '%s'\n", i, msg);
      exit(0);
    }
  }

  for (int i = 0; i < NCHILD; i++) {
    wait(0);
  }

  printf("PARENT: final log:\n");
  int slot_size = size / NCHILD;
  for (int i = 0; i < NCHILD; i++) {
    char *slot = log + (i * slot_size);
    printf("  [%d] %s\n", i, slot);
  }

  exit(0);
}

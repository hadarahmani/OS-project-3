#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid;
  int ppid = getpid();
  char *shmem;
  int size = 4096;

  pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // CHILD
    printf("child: sz before = %d\n", sbrk(0));

    // הקצה מקום חדש אצל הילד
    char *child_buf = sbrk(size);

    shmem = (char*) map_shared_pages(child_buf, ppid, size);
    if ((uint64)shmem == (uint64)-1) {
      printf("child: map_shared_pages failed\n");
      exit(1);
    }

    printf("child: sz after mapping = %d\n", sbrk(0));
    strcpy(shmem, "Hello daddy!");
    printf("child wrote: %s\n", shmem);
    printf("child: exiting\n");
    exit(0);

  } else {
    wait(0);

    // ההורה מקצה מקום משלו
    char *parent_buf = sbrk(size);

    shmem = (char*) map_shared_pages(parent_buf, pid, size);
    if ((uint64)shmem == (uint64)-1) {
      printf("parent: map_shared_pages failed\n");
      exit(1);
    }

    printf("parent read: %s\n", shmem);
    exit(0);
  }
}

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int parent = getpid();
  int size = 4096;

  // הורה מקצה buffer
  char *parent_buf = sbrk(size);
  printf("PARENT: pid=%d buf=%p\n", parent, parent_buf);

  int pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    // === CHILD ===
    int mypid = getpid();
    printf("CHILD: pid=%d ppid=%d\n", mypid, parent);
    printf("CHILD: sz before = %d\n", sbrk(0));

    // הילד מגדיל sz כדי שיהיה מקום
    sbrk(size);

    // מבצע map מההורה לכתובת חדשה אצלו
    char *shmem = (char*) map_shared_pages(parent_buf, parent, size);
    printf("CHILD: got shmem = %p\n", shmem);

    if ((uint64)shmem == (uint64)-1) {
      printf("CHILD: map_shared_pages failed\n");
      exit(1);
    }

    strcpy(shmem, "Hello daddy!");
    printf("CHILD: wrote: '%s'\n", shmem);

    printf("CHILD: sz after = %d\n", sbrk(0));
    printf("CHILD: exiting\n");
    exit(0);

  } else {
    // === PARENT ===
    wait(0);

    printf("PARENT: after wait, buf content is: '%s'\n", parent_buf);
    printf("PARENT: buf addr is: %p\n", parent_buf);
    printf("PARENT: done\n");
    exit(0);
  }
}

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NCHILD 4
#define MSGMAX 64
#define PGSIZE 4096

// Converts integer to string
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

// Align pointer to next 4-byte boundary
char *align4(char *p) {
  return (char *)(((uint64)p + 3) & ~3);
}

// Encode: upper 16 bits = child index, lower 16 bits = length
uint32 encode_header(int index, int len) {
  return ((index & 0xFFFF) << 16) | (len & 0xFFFF);
}

int get_index(uint32 header) {
  return (header >> 16) & 0xFFFF;
}

int get_length(uint32 header) {
  return header & 0xFFFF;
}

int main(int argc, char *argv[]) {
  int parent = getpid();
  char *log = sbrk(PGSIZE);  // Allocate shared buffer in parent

  printf("PARENT: pid=%d log=%p\n", parent, log);

  for (int i = 0; i < NCHILD; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }

    if (pid == 0) {
      // CHILD
      char *child_log = (char *)map_shared_pages(log, parent, PGSIZE);
      if ((uint64)child_log == (uint64)-1) {
        printf("CHILD %d: map_shared_pages failed\n", i);
        exit(1);
      }

      // Same message for all children
      char msg[MSGMAX] = "Hello from child ";
      itoa(i, msg + strlen(msg));
      int msg_len = strlen(msg);

      char *p = child_log;
      char *end = child_log + PGSIZE;

      while (1) {
        p = align4(p);
        if (p + 4 + msg_len >= end) {
          // Buffer full or no room for this message: just exit
          exit(0);
        }

        uint32 *hdr = (uint32 *)p;
        uint32 old = __sync_val_compare_and_swap(hdr, 0, encode_header(i, msg_len));
        if (old == 0) {
          // Claimed slot
          memmove(p + 4, msg, msg_len);
          break;
        }

        // Skip past this message
        p += 4 + get_length(old);
      }
      exit(0);
    }
  }

  // Parent waits for children
  for (int i = 0; i < NCHILD; i++)
    wait(0);

  // Read from shared buffer
  printf("PARENT: log contents:\n");
  char *p = log;
  char *end = log + PGSIZE;

  while (1) {
    p = align4(p);
    if (p + 4 > end) break;

    uint32 header = *(uint32 *)p;
    if (header == 0) {
      p += 4;
      continue;
    }

    int idx = get_index(header);
    int len = get_length(header);
    if (p + 4 + len > end) break;

    char msg[MSGMAX];
    memmove(msg, p + 4, len);
    msg[len] = '\0';

    printf("  Child %d: %s\n", idx, msg);
    p += 4 + len;
  }

  exit(0);
}

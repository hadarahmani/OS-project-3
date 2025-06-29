#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/memlayout.h"
#include <stdint.h>

#define SHMEM_SIZE 4096 // size of a memory page (4KB)
#define MSG "Hello daddy"
#define GET_SIZE sbrk(0)

void child_program(int disable_unmap, int parent_pid, void* parent_va) {
    printf("---- CHILD PROCESS START ----\n");

    // Before mapping
    printf("Child Process: size before mapping is %p\n", GET_SIZE);

    // Map shared memory from parent
    void* child_va = (void*)(map_shared_pages((uint64)parent_va, SHMEM_SIZE));

    if ((uint64)child_va == 0) 
    {
        printf("Child Process: map_shared_pages failed\n");
        exit(1);
    }

    // Write to shared memory
    strcpy((char*)child_va, MSG);

    printf("Child Process: wrote %s to the shared memory\n", (char*)child_va);
    printf("Child Process: size after mapping is %p\n", GET_SIZE);

    // if the disable_unmap flag is set, skip unmapping
    if (!disable_unmap) 
    {
        if (unmap_shared_pages((uint64)child_va, SHMEM_SIZE) < 0) {
            printf("Child Process: unmap_shared_pages failed\n");
            exit(1);
        }

        printf("Child Process: size after unmapping is %p\n", GET_SIZE);
    } 

    else 
    {
        printf("Child Process: skipping unmap shared pages due to disable_unmap flag\n");
    }

    // Test heap after unmapping 
    // to show that we can allocate memory properly in the child process after unmapping shared memory
    void* test_malloc = malloc(SHMEM_SIZE * 1000);
    printf("Child Process: testing malloc after unmapping shared memory\n");

    if (test_malloc) 
    {
        strcpy((char*)test_malloc, "Child memory allocation succeeded");
        printf("Child Process: wrote %s to the allocated memory\n", (char*)test_malloc);
    } 

    else 
    {
        printf("Child Process: malloc failed\n");
    }

    printf("Child Process: size after malloc is %p\n", GET_SIZE);
    free(test_malloc);
    exit(0);
}

void parent_program(void* parent_va) 
{
    wait(0);  // Wait for child process to finish
    printf("---- PARENT PROCESS START ----\n");

    printf("Parent Process: got the message %s from the child process\n", (char*)parent_va);

    if (unmap_shared_pages((uint64)parent_va, SHMEM_SIZE) < 0) 
    {
        printf("Parent Process: unmap_shared_pages failed\n");
    }

    printf("Parent Process: finish testing shared memory\n");
    exit(0);
}

void test_shared_memory(int disable_unmap) 
{
    int parent_pid = getpid();
    void* parent_va = malloc(SHMEM_SIZE);

    if (parent_va == 0) 
    {
        printf("Parent Process: malloc failed\n");
        exit(1);
    }

    printf("Parent Process: size before fork is %p\n", GET_SIZE);

    int pid = fork();

    if (pid < 0) 
    {
        printf("Fork failed\n");
        exit(1);
    }

    if (pid == 0) 
    {
        child_program(disable_unmap, parent_pid, parent_va);
    } 

    else 
    {
        parent_program(parent_va);
    }
}

int main(int argc, char *argv[]) 
{
    int disable_unmap = 0;
    if (argc > 1 && strcmp(argv[1], "disable_unmap") == 0) 
    {
        disable_unmap = 1;
    }

    test_shared_memory(disable_unmap);
    exit(0);
}

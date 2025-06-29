#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/memlayout.h"
#include <stdint.h>
#include <stdbool.h>


#define SHMEM_SIZE 4096 // size of a memory page (4KB)    
#define NUM_CHILDREN 4 // number of child processes to create           
#define MAX_MESSAGE_LENGTH 64         // max message length

// this function concatenates src to dst and returns the new length of dst
int my_strcat(char* dst, const char* src)
{
    int dst_len = 0;
    while (dst[dst_len] != '\0') dst_len++;

    int i = 0;
    while (src[i] != '\0')
    {
        dst[dst_len + i] = src[i];
        i++;
    }
    dst[dst_len + i] = '\0';

    return dst_len + i;  // return new length of string (excluding null terminator)
}

//this function converts an integer to a string and appends it to buf at the specified offset
int my_atoi(char* buf, int offset, int num)
{
    if (num == 0)
    {
        buf[offset++] = '0';
        return offset;
    }

    char temp[10];
    int len = 0;

    while (num > 0 && len < sizeof(temp))
    {
        temp[len++] = '0' + (num % 10);
        num /= 10;
    }

    for (int i = len - 1; i >= 0; i--)
    {
        buf[offset++] = temp[i];
    }

    return offset;
}

// this function copies the string from src to dst and returns the length of the copied string
int my_strcpy(char* dst, const char* src)
{
    int i = 0;
    while (src[i] != '\0')  // Loop until the end of src string
    {
        dst[i] = src[i];    // Copy each character from src to dest
        i++;
    }
    dst[i] = '\0';          // Copy the null terminator to dest
    return i;               // Return the number of characters copied (excluding the null terminator)
}

void child_process(int child_idx, void *parent_va)
{
    char msg[MAX_MESSAGE_LENGTH];
    char stars[child_idx+1];
    memset(stars, '*', child_idx);
    stars[child_idx] = '\0'; // Null-terminate the string

    int pos = 0;
    pos = my_strcpy(msg, "Child Process ");
    pos = my_atoi(msg, pos, child_idx);

    pos = my_strcat(msg, ": hello ");
    pos = my_strcpy(msg + pos, stars);

    void* buf = (void*)map_shared_pages((uint64) parent_va, SHMEM_SIZE);   

    char* va = (char*)buf + 4; // save 4 bytes for finished child counter
    uint16 message_length = strlen(msg) + 1; // +1 for null terminator
    uint64 slot_size = message_length + 4; // 4 bytes for the header 

    // ensuring current position with added value don't overflow
    while((uint64)va + slot_size < (uint64)buf + SHMEM_SIZE)
    {
        uint32* header =  (uint32*)va; // Initialize header with the current position
        uint32 haeder_data = (child_idx << 16) | message_length; // child_id in upper 16 bits, message_length in lower 16 bits

        if (__sync_val_compare_and_swap(header, 0, haeder_data) == 0)
        {
            memcpy(va + 4, msg, message_length);

            __sync_or_and_fetch(header, (1 << 31)); // Set the valid bit in the header
        
            sleep(1); // give cpu to other processes
        }

        uint16 curr_msg_len = (*header) & 0xFFFF; 
        va += 4 + curr_msg_len; // Move to the next slot
        va = (char*)(((uint64)va + 3) & ~3); // Align to 4 bytes
    }

    __sync_fetch_and_add((uint32 *)buf, 1); // Increment the finished children counter
    exit(0);
}

void parent_process(void *buf, int num_children)
{
    uint32* finished_children = (uint32*) buf; // Pointer to the finished child counter
    uint64 end = (uint64)buf + SHMEM_SIZE;

    while (true)
    {
        char* addr = (char*)buf + 4; // Start reading from the first message slot

        while ((uint64)addr + 4 < end)  // Must have space for at least header
        {
            uint32 header = *(uint32*)addr;

            if((header >> 31) != 1)
            {
                addr += 4; // Skip empty slots
                addr = (char*)(((uint64)addr + 3) & ~3); // Align to 4 bytes
                continue;
            }
           
            uint16 child_id = (header >> 16) & 0x7FFF; // Extract child ID from the header
            uint32 msg_len = header & 0xFFFF; // Extract message length from the header

            //if (child_id >= 1 && child_id <= num_children && !num_read[child_id - 1])
            if (child_id >= 1 && child_id <= num_children) 
            {
                char msg[MAX_MESSAGE_LENGTH + 1] = {0};
                memcpy(msg, addr + 4, msg_len < MAX_MESSAGE_LENGTH ? msg_len : MAX_MESSAGE_LENGTH);
                msg[msg_len] = '\0'; // Null-terminate the message

                printf("Parent Process: Child Process %d wrote %s\n", child_id, msg);

                *(uint32*) addr &= ~(1 << 31); // Clear the valid bit in the header
            }
            addr += 4 + msg_len; // Move to the next message slot
            addr = (char*)(((uint64)addr + 3) & ~3);
        }

        if (*finished_children >= num_children)
        {
            break;
        }
    }

    printf("Parent Process: All messages read from children\n");
    exit(0);
}


int main(int argc, char *argv[])
{
    void *parent_va = malloc(SHMEM_SIZE);
    memset(parent_va, 0, SHMEM_SIZE); // Initialize shared memory to zero

    int num_children = NUM_CHILDREN; // Default number of child processes

    if (argc > 1)
    {
        num_children = atoi(argv[1]);
    }

    for (int i = 0; i < num_children; i++)
    {
        int pid = fork();

        if (pid < 0)
        {
            printf("Parent Process: Fork failed\n");
            exit(1);
        }

        // child process
        if (pid == 0)
        {
            child_process(i+1, parent_va);
        }
    }

    parent_process(parent_va, num_children);
    return 0;
}
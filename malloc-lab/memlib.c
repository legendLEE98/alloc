/*
 * memlib.c - a module that simulates the memory system.  Needed because it 
 *            allows us to interleave calls from the student's malloc package 
 *            with the system's malloc package in libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* private variables */
static char *mem_start_brk;  // heap 어디부터 시작했는지
static char *mem_brk;        // heap 현재 어디까지 할당을 시켜놨는지의 주소값
static char *mem_max_addr;   // heap 이 최대 할당할 수 있는 메모리 영역

/* 
 * mem_init - initialize the memory system model
 */

void mem_init(void)
{
    // mem_start_brk => NULL 이라면 공간 자체를 할당하지 못한다는 의미
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
        // 메모리 오류를 반환하고 프로그램 실행을 종료시켜버립니다.
        fprintf(stderr, "mem_init_vm: malloc error\n");
        exit(1);
    }

    // max_heap + 시작 메모리주소 == 종료지점
    mem_max_addr = mem_start_brk + MAX_HEAP;  /* max legal heap address */
    // mem_brk를 초기화 시킴
    mem_brk = mem_start_brk;                  /* heap is empty initially */
}

/* 
 * mem_deinit - free the storage used by the memory system model
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/* 
 * mem_sbrk - simple model of the sbrk function. Extends the heap 
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 */

// os한테 힙 영역을 할당 받음. 
// mem_start_brk 이녀석 범위 안에서 추가할 수 있다
// int incr == 받은 매개변수 만큼 메모리 추가 할당 요청
void *mem_sbrk(int incr) 
{
    // old_brk정의 = mem_brk;
    char *old_brk = mem_brk;

    // incr 0 || mem_brk + incr > 메모리값보다 크면
    if ((incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
        errno = ENOMEM;
        fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
        return (void *)-1;
    }

    // mem_brk += incr 값을 추가하면 
    mem_brk += incr;
    // old_brk의 주소를 리턴합니다.
    return (void *)old_brk;
}

/*
 * mem_heap_lo - return address of the first heap byte
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/* 
 * mem_heap_hi - return address of last heap byte
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - returns the heap size in bytes
 */
size_t mem_heapsize() 
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - returns the page size of the system
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}

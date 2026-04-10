/*
 * mm-naive.c - 가장 빠르지만 메모리 효율이 낮은 malloc 패키지.
 *
 * 이 naive 구현에서는 brk 포인터를 증가시키는 방식으로 블록을 할당합니다.
 * 블록은 순수한 payload로만 구성되며, 헤더와 푸터가 없습니다.
 * 블록은 병합(coalesce)되거나 재사용되지 않습니다.
 * realloc은 mm_malloc과 mm_free를 이용해 직접 구현되어 있습니다.
 *
 * 학생 노트: 이 헤더 주석을 본인의 구현 방식에 대한 설명으로 교체하세요.
 * 테스트
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h" 

/*********************************************************
 * 학생 노트: 아래 구조체에 팀 정보를 입력하세요.
 ********************************************************/
team_t team = {
    /* 팀 이름 */
    "ateam",
    /* 첫 번째 멤버 이름 */
    "Harry Bovik",
    /* 첫 번째 멤버 이메일 */
    "bovik@cs.cmu.edu",
    /* 두 번째 멤버 이름 (없으면 빈칸) */
    "",
    /* 두 번째 멤버 이메일 (없으면 빈칸) */
    ""
};

/* 단일 워드(4) 또는 더블 워드(8) 정렬 */
#define ALIGNMENT 8

/* ALIGNMENT의 배수로 올림 */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*
 * mm_init - malloc 패키지를 초기화합니다.
 */
int mm_init(void)
{
    return 0;
}

/*
 * mm_malloc - brk 포인터를 증가시켜 블록을 할당합니다.
 *             항상 alignment의 배수 크기로 블록을 할당합니다.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - 블록을 해제합니다. (현재 아무 동작도 하지 않음)
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - mm_malloc과 mm_free를 이용해 단순하게 구현한 realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
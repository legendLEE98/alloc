/*
 * mm-naive.c - 가장 빠르지만 메모리 효율이 낮은 malloc 패키지.
 *
 * 이 naive 구현에서는 brk 포인터를 증가시키는 방식으로 블록을 할당합니다.
 * 블록은 순수한 payload로만 구성되며, 헤더와 푸터가 없습니다.
 * 블록은 병합(coalesce)되거나 재사용되지 않습니다.
 * realloc은 mm_malloc과 mm_free를 이용해 직접 구현되어 있습니다.
 *
 * 학생 노트: 이 헤더 주석을 본인의 구현 방식에 대한 설명으로 교체하세요.
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

// 여기는 csapp에 나와있는 implicit(묵시적) 방법
#define WSIZE 4 // 4바이트 사이즈
#define DSIZE 8 // 8바이트 사이즈

// 반환값 4096
#define CHUNKSIZE (1<<12)

// 크기 비교
#define MAX(x, y) ((x) > (y)? (x) : (y)) 
// ((size | alloc)) 반환 - byte는 8단위고, 8단위 아래 3개는 비트 남으니까 alloc 상태값까지 더함.
#define PACK(size, alloc) ((size) | (alloc))

// p 양수로 읽기
#define GET(p) (*(unsigned int *)(p))
// p값 val로 바꾸기
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// 앰퍼샌드 == AND 연산자 | ~는 반대로 변환
// 0x7 == 0000 0111 / ~0x7 == 1111 1000
// 즉, 1111 1 을 시작지점으로 잡으면, 8이 몇 개 할당되었는지로 사이즈를 구할 수 있음.
#define GET_SIZE(p) (GET(p) & ~0x7)
// P의 첫 번째 자리 가져오기
#define GET_ALLOC(p) (GET(p) & 0x1)

// bp 블록 포인터 위치 주소
// hdrp는 해당 bp의 헤드의 주소 (시작지점)
#define HDRP(bp) ((char *)(bp) - WSIZE)
// getSize는 헤당 블록의 최대 크기 + 페이로드 위치 = 4 + n - 8. 즉 푸터의 위치
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 다음 블럭의 페이로드 위치
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
// 이전 블럭의 푸터의 사이즈를 보고 해당 사이즈만큼 현재 페이로드에서 뺌.
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// 아래는 naive malloc 구현방식 (동등한 같은 의미로 쓰임)
/* 단일 워드(4) 또는 더블 워드(8) 정렬 */
// #define ALIGNMENT 8

// /* ALIGNMENT의 배수로 올림 */
// #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 주소 선언
char * heap_listp;

// static 함수 선언
static void *extend_heap(size_t words);
static void *coalesce(void *bp);

/*
 * mm_init - malloc 패키지를 초기화합니다.
 */
int mm_init(void)
{
    // heap_listp를 전역으로 선언하면 0x0000이 나옴
    // 여기에서 해당 주소값이 0xfffff일 경우((void*)-1) 인 경우면 return -1을 함.
    // mem_sbrk가 할당 불가능하단 의미
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    // 해당 주소의 값 0으로 초기화
    PUT(heap_listp, 0);
    
    // ㅡㅡㅡ 얘네는 가드 역할(넘어가지 말란 의미로 사용) ㅡㅡㅡㅡ
    // 프롤로그 영역의 헤더 선언
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    // 프롤로그 영역의 푸터 선언
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    // 에필로그 영역의 헤더 선언
    PUT(heap_listp + (3*WSIZE), (PACK(0, 1)));

    // heap의 실제 시작지점을 프롤로그 영역으로 지정
    // 프롤로그 푸터 위치 존재. next_blkp로 다음 영역부터 찾기
    heap_listp += (2*WSIZE);

    // extend_heap(size_t words)
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    
    return 0;
}

// heap 연장 기능
// words 사이즈 연장을 위해서 매개변수 받아냄
static void *extend_heap(size_t words) {
    // 블록 포인터 선언
    char * bp;
    // 크기 선언
    size_t size;

    // 크기 -> 항상 짝수로 맞춰주기
    // 이거 init할 땐 의미 없는데, 뒤에 malloc 쓸 때 의미가 있어지나봄
    // 일단 스킵
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    
    // 흐름으로 볼 때, 처음 하나의 큰 블럭으로 받아두고, 필요할 때 마다 쪼개서 쓰는 방식이라고 함.
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(size, 0));

    // 양쪽 남는 영역 있으면 합치는 애
    return coalesce(bp);
}

/*
 * mm_free - 블록을 해제합니다. (현재 아무 동작도 하지 않음)
 */
void mm_free(void *bp)
{
    // size 크기를 bp 포인터의 크기와 동일하게 만듦.
    size_t size = GET_SIZE(HDRP(bp));

    // 입력받은 bp의 크기는 냅두고, 할당상태는 0으로 만듦 (헤더 푸터 둘 다)
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}

static void *coalesce(void *bp)
{
    
}

/*
 * mm_malloc - brk 포인터를 증가시켜 블록을 할당합니다.
 *             항상 alignment의 배수 크기로 블록을 할당합니다.
 */
void *mm_malloc(size_t size)
{

}

/*
 * mm_realloc - mm_malloc과 mm_free를 이용해 단순하게 구현한 realloc
 */
void *mm_realloc(void *ptr, size_t size)
{

}
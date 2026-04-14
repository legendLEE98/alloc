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
// hdrp는 해당 bp의 헤더의 주소 (시작지점)
#define HDRP(bp) ((char *)(bp) - WSIZE)
// getSize는 헤당 블록의 최대 크기 + 페이로드 위치 = 4 + n - 8. 즉 현재 푸터의 위치
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 다음 블럭 페이로드의 위치
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
// 이전 블럭의 푸터의 사이즈를 보고 해당 사이즈만큼 현재 페이로드에서 뺌.
// 즉 이전 페이로드의 위치
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

// 힙 연장 기능
static void *extend_heap(size_t words);
// free list 정렬 기능
static void *coalesce(void *bp);
// first fit으로 찾기
static void *find_fit(size_t asize);
// 힙 연장 후 재할당 기능 추가
static void place(void * bp, size_t asize);

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
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

/*
 * mm_free - 블록을 해제합니다.
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

// 양쪽 남는 영역 있으면 합치는 애
static void *coalesce(void *bp)
{
    // 이전, 다음 alloc이 사용중인지 확인하는 기능
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 현재 크기 bp의 사이즈 확인
    size_t size = GET_SIZE(HDRP(bp));

    // 조건문
    // prev, next 둘 다 사용 중인 경우 ( 1, 1 인 경우)
    if (prev_alloc && next_alloc)
        return bp;
    // next만 비었을 경우
    // 해야 할 것
    // 헤더 위치는 고정에, 푸터의 위치만 바뀌면 됨.
    else if (prev_alloc && !next_alloc) {
        // 현재 크기 + 다음 공간 사이즈
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // prev만 비었을 경우
    // 해야 할 것
    // size는 일단 프리뷰 블럭만큼 키우고, 푸터 먼저 서언
    // 헤더 위치 구하고, bp값 이전 헤더 위치로 변경
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // 둘 다 비었을 경우
    // 해야 할 것
    // 위에 한거 두 개
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * mm_malloc - brk 포인터를 증가시켜 블록을 할당합니다.
 *             항상 alignment의 배수 크기로 블록을 할당합니다.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendSize;
    char *bp;

    // 할당 요청인데 사이즈가 없으면 반환해야함.
    if (size == 0)
        return NULL;
    
    // size보다 DSIZE가 더 크면
    // 헤더 푸터 포함해서 딱 2만 전달해주면 됨.
    if (size <= DSIZE)
        asize = 2*DSIZE;
    // 그 외의 경우
    else 
        // DSIZE * (크기 + 8(헤더푸터) + (DSIZE-1)) / DSIZE
        // 이거 15대입하면 padding 1 / 16대입하면 padding 0임 짱인데
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    
    // free list인 블록 찾기
    // asize에 맞는 bp 포인터 위치 찾기
    if ((bp = find_fit(asize))) {
        // 장소 선정. bp랑 asize만큼 제공
        place(bp, asize);
        return bp;
    }

    // asize 혹은 청크 크기만큼 할당
    // 힙 추가로 열어달라고 요청
    extendSize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendSize/WSIZE)) == NULL)
        return NULL;

    // 연장요청 해왔으니까 bp 공간 다시 할당해줘야 함.
    // asize = 요청 사이즈를 얼라이먼트에 맞춰서 할당해준 필요값
    place(bp, asize);

    return bp;
}

// first fit으로 찾아야 함.
// 순회하면서 맞는 값 찾는거임
static void *find_fit(size_t asize) {
    // 임시 temp선언
    void *bp;

    // next blkp로 순회, 해당 블럭의 사이즈가 0 보다 클 때 까지
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // 해당 블럭이 allocate 상태가 아니면서, 입력받은 사이즈보다 getSize가 크거나 같을 때
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;
}

static void *next_fit(size_t asize) {
    // 임시 temp선언
    void *bp;

    // next blkp로 순회, 해당 블럭의 사이즈가 0 보다 클 때 까지
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // 해당 블럭이 allocate 상태가 아니면서, 입력받은 사이즈보다 getSize가 크거나 같을 때
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL;
}

// 헤더 푸터 포함해서 최소 16의 공간이 있어야 함.
// bp - 포인터 위치 / asize - 유저가 요청하는 블럭 단위
static void place(void * bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_realloc - mm_malloc과 mm_free를 이용해 단순하게 구현한 realloc
 */
void *mm_realloc(void *bp, size_t size)
{
    // bp가 NULL이면 그냥 malloc
    if (bp == NULL)
        return mm_malloc(size);
    
    // size가 0이면 그냥 free
    if (size == 0) {
        mm_free(bp);
        return NULL;
    }

    // 새 공간 할당
    void *newbp = mm_malloc(size);
    if (newbp == NULL)
        return NULL;

    // 기존 데이터 복사
    size_t oldsize = GET_SIZE(HDRP(bp)) - DSIZE;  // 헤더+푸터 빼고
    if (size < oldsize)
        oldsize = size;  // 줄어드는 경우 새 크기만큼만 복사
    memcpy(newbp, bp, oldsize);

    mm_free(bp);
    return newbp;
}
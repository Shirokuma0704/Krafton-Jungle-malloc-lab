/*
 * mm-naive.c - 제일 빠르지만 메모리는 제일 낭비하는 malloc 패키지예요.
 *
 * 이 단순한 방식은 brk 포인터를 올리기만 해서 블록을 할당해요.
 * 블록은 전부 payload이고, 헤더도 푸터도 없어요. 블록을 합치지도
 * 다시 쓰지도 않아요. realloc은 mm_malloc과 mm_free로 바로 만들었어요.
 * (CSAPP 9.9.5에 나오는 "제일 단순한 할당기"가 바로 이거예요)
 *
 * 학생에게: 이 머리 주석은 지우고, 내 풀이가 어떤 방식인지
 * 큰 그림으로 설명하는 주석으로 바꿔 주세요.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * 학생에게: 다른 걸 하기 전에 아래 구조체에
 * 팀 정보를 먼저 채워 주세요. (mdriver가 이 칸을 검사해요)
 ********************************************************/
team_t team = {
    /* 팀 이름 */
    "ateam",
    /* 첫 번째 팀원 이름 */
    "Harry Bovik",
    /* 첫 번째 팀원 이메일 */
    "bovik@cs.cmu.edu",
    /* 두 번째 팀원 이름 (없으면 비워 두기) */
    "",
    /* 두 번째 팀원 이메일 (없으면 비워 두기) */
    ""
};


static char *heap_listp;
// 가용리스트 헤드
static char *free_listp;

/* 싱글 워드(4) 또는 더블 워드(8) 정렬 */
#define ALIGNMENT 8

/* size를 ALIGNMENT의 배수로 올려요 */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// 기본 상수 및 매크로 설정

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_ADDRESS(p) (*(char **)(p))
#define PUT_ADDRESS(slot, target) (*(unsigned int *)(slot) = (unsigned int)(target))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HEADER_ADDRESS(bp) ((char *)(bp) - WSIZE)
#define FOOTER_ADDRESS(bp) ((char *)(bp) + GET_SIZE(HEADER_ADDRESS(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PRED(bp) ((char *)(bp))
#define SUCC(bp) ((char *)(bp) + WSIZE)

/* size_t 하나를 담을 칸의 크기를 정렬에 맞춘 값이에요.
   아래 mm_malloc과 mm_realloc이 블록 맨 앞에 요청 크기를 적어 두는 데 써요 */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


// free 블럭들의경계를 지워 큰 free 블럭으로 만드는 병합함수
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FOOTER_ADDRESS(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HEADER_ADDRESS(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HEADER_ADDRESS(bp));

    char *head = free_listp;


    // 앞뒤 둘다 배정됨
    if (prev_alloc && next_alloc)
    {
        if (head == NULL)
        {
            PUT_ADDRESS(SUCC(bp), NULL);
            PUT_ADDRESS(PRED(bp), NULL);
            free_listp = bp;
            return bp;
        }

        PUT_ADDRESS(PRED(bp), NULL);
        PUT_ADDRESS(SUCC(bp), head);
        PUT_ADDRESS(PRED(head), bp);
        free_listp = bp;

        return bp;
    }
    // 뒤가 풀려있음
    else if (prev_alloc && !next_alloc)
    {
        char *pred = GET_ADDRESS(PRED(NEXT_BLKP(bp)));
        char *succ = GET_ADDRESS(SUCC(NEXT_BLKP(bp)));

        size += GET_SIZE(HEADER_ADDRESS(NEXT_BLKP(bp)));
        PUT(HEADER_ADDRESS(bp), PACK(size, 0));
        PUT(FOOTER_ADDRESS(bp), PACK(size, 0));
        // 새블럭을 가용 리스트 삽입
        PUT_ADDRESS(PRED(bp), NULL);
        PUT_ADDRESS(SUCC(bp), head);
        PUT_ADDRESS(PRED(head), bp);
        // 합치기전 블럭을 빼고 이어줌
        if (succ != NULL) PUT_ADDRESS(SUCC(succ), pred);
        if (pred != NULL) PUT_ADDRESS(PRED(pred), succ);
    }
    // 앞이 풀려있음
    else if (!prev_alloc && next_alloc)
    {
        char *pred = GET_ADDRESS(PRED(PREV_BLKP(bp)));
        char *succ = GET_ADDRESS(SUCC(PREV_BLKP(bp)));

        size += GET_SIZE(HEADER_ADDRESS(PREV_BLKP(bp)));
        PUT(FOOTER_ADDRESS(bp), PACK(size, 0));
        PUT(HEADER_ADDRESS(PREV_BLKP(bp)),PACK(size, 0));

        bp = PREV_BLKP(bp);

        // 새블럭을 가용 리스트 삽입
        PUT_ADDRESS(PRED(bp), NULL);
        PUT_ADDRESS(SUCC(bp), head);
        PUT_ADDRESS(PRED(head), bp);
        // 합치기전 블럭을 빼고 이어줌
        if (succ != NULL) PUT_ADDRESS(SUCC(succ), pred);
        if (pred != NULL) PUT_ADDRESS(PRED(pred), succ);


    }
    // 양쪽 다 풀려있음
    else
    {
        char *prev_pred = GET_ADDRESS(PRED(PREV_BLKP(bp)));
        char *prev_succ = GET_ADDRESS(SUCC(PREV_BLKP(bp)));
        char *next_pred = GET_ADDRESS(PRED(NEXT_BLKP(bp)));
        char *next_succ = GET_ADDRESS(SUCC(NEXT_BLKP(bp)));

        size += GET_SIZE((FOOTER_ADDRESS(PREV_BLKP(bp)))) + GET_SIZE(FOOTER_ADDRESS(NEXT_BLKP(bp)));
        PUT(HEADER_ADDRESS((PREV_BLKP(bp))), PACK(size, 0));
        PUT(FOOTER_ADDRESS((NEXT_BLKP(bp))), PACK(size, 0));

        PUT_ADDRESS(PRED(next_pred), next_succ);
        PUT_ADDRESS(SUCC(next_succ), next_pred);
        PUT_ADDRESS(PRED(prev_pred), prev_succ);
        PUT_ADDRESS(SUCC(prev_succ), prev_pred);

        bp = PREV_BLKP(bp);

        PUT_ADDRESS(PRED(bp), NULL);
        PUT_ADDRESS(SUCC(bp), head);
        PUT_ADDRESS(PRED(head), bp);

    }
    free_listp = bp;
    return bp;
}


static void *extend_heap(size_t words)
{
    char *bp;
    size_t byte_size;


    // 홀수 패딩 추가후, word를 바이트 단위로 변환
    byte_size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    // 힙을 더 늘릴수 없을때 NULL 반환
    if ((long)(bp = mem_sbrk(byte_size)) == -1)
        return NULL;

    // 헤더 푸터
    PUT(HEADER_ADDRESS(bp), PACK(byte_size, 0));
    PUT(FOOTER_ADDRESS(bp), PACK(byte_size, 0));

    // 에필로그
    PUT(HEADER_ADDRESS(NEXT_BLKP(bp)), PACK(0,1));

    return coalesce(bp);
}

static void* find_fit(int asize)
{
    char *head = free_listp;

    while (SUCC(head) != NULL) // 가용리스트 끝에 도달할때 까지
    {
        if (GET_SIZE(HEADER_ADDRESS(head)) + 16 >= asize)
            return head;
        head = GET_ADDRESS(SUCC(head));
    }
    return NULL;
}

static void place(void *bp, int size)
{
    int free_size = GET_SIZE(HEADER_ADDRESS(bp)) - size;
    char *succ = GET_ADDRESS(SUCC(bp));
    char *prev = GET_ADDRESS(PRED(bp));

    if (free_size == 0)
    {
        PUT(HEADER_ADDRESS(bp), PACK(size, 1));
        PUT(FOOTER_ADDRESS(bp), PACK(size, 1));

        if (free_listp == bp) free_listp = succ;
        if (succ != NULL) PUT_ADDRESS(succ, prev);
        if (prev != NULL) PUT_ADDRESS(prev, succ);
        return;
    }

    char *free_ft = FOOTER_ADDRESS(bp);
    PUT(HEADER_ADDRESS(bp), PACK(size, 1));
    PUT(FOOTER_ADDRESS(bp), PACK(size, 1));

    PUT(FOOTER_ADDRESS(bp) + WSIZE, PACK(free_size,0));
    PUT(free_ft, PACK(free_size,0));

    char *new_bp = FOOTER_ADDRESS(bp) + (WSIZE *2);

    PUT_ADDRESS(PRED(new_bp), NULL);
    PUT_ADDRESS(SUCC(new_bp), free_listp);
    PUT_ADDRESS(PRED(free_listp), new_bp);

    free_listp = new_bp;
    if (succ != NULL) PUT_ADDRESS(succ, prev);
    if (prev != NULL) PUT_ADDRESS(prev, succ);

}


/*
 * mm_init - malloc 패키지를 초기화해요.
 *     지금은 아무것도 안 하고 0(성공)만 돌려줘요.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) - 1)
        return -1;

    // 프롤로그 에필로그 세팅
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    heap_listp += (2 * WSIZE);

    // 가용 리스트 초기화
    free_listp = NULL;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}


/*
 * mm_malloc - brk 포인터를 올려서 블록을 하나 할당해요.
 *     블록 크기는 항상 정렬 단위(8)의 배수예요.
 *     블록 맨 앞 칸에 요청 크기를 적어 두고, 그 다음 주소를 돌려줘요.
 */
void *mm_malloc(size_t size)
{
    size_t asize, extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    // 워드가 2워드 이하일때 헤더 내용(패딩) 푸터 -> 최소 4워드
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    // 나머지 경우 7을 더한뒤 8을 나눈뒤 곱해 올림
    else asize = ALIGN(size + DSIZE);

    // 크기가 맞다면 늘림
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    // asize와 힙을 늘리는 최소사이즈중 큰값 고름
    extendsize = MAX(asize, CHUNKSIZE);

    // 힙 확장 가능 검사 + if 문으로 확장과 bp변경.
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    //
    place(bp, asize);
    return bp;
}

/*
 * mm_free - 블록을 해제해도 아무 일도 안 일어나요.
 *     그래서 메모리를 다시 못 쓰고, 큰 트레이스에서 out of memory가 나요.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HEADER_ADDRESS(bp));
    // 헤더 푸터 0으로 변환
    PUT(HEADER_ADDRESS(bp), PACK(size, 0));
    PUT(FOOTER_ADDRESS(bp), PACK(size, 0));
    // free 정렬
    coalesce(bp);
}


/*
 * mm_realloc - mm_malloc과 mm_free만으로 단순하게 만들었어요.
 *     새 블록을 받고, 옛 블록 맨 앞에 적어 둔 크기만큼 내용을 복사한 뒤,
 *     옛 블록을 해제해요.
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HEADER_ADDRESS(ptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}















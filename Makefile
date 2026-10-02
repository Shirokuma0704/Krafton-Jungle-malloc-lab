#
# Students' Makefile for the Malloc Lab
#
TEAM = bovik
VERSION = 1
HANDINDIR = /afs/cs.cmu.edu/academic/class/15213-f01/malloclab/handin

CC = gcc
CFLAGS = -Wall -O2 -m32

# 채점 드라이버(mdriver.c)와 타이머 소스는 support/ 로 옮겨 뒀어요.
# VPATH 로 make 가 거기서 .c/.h 를 찾고, CPPFLAGS 로 컴파일러가 헤더를 찾아요.
# (CFLAGS 가 아니라 CPPFLAGS 에 둔 건, 디버그 빌드가 CFLAGS 를 통째로 바꿔 끼워도 경로가 안 빠지게 하려는 거예요)
VPATH = support
CPPFLAGS = -I. -Isupport

OBJS = mdriver.o mm.o memlib.o fsecs.o fcyc.o clock.o ftimer.o

mdriver: $(OBJS)
	$(CC) $(CFLAGS) -o mdriver $(OBJS)

mdriver.o: mdriver.c fsecs.h fcyc.h clock.h memlib.h config.h mm.h
memlib.o: memlib.c memlib.h
mm.o: mm.c mm.h memlib.h
fsecs.o: fsecs.c fsecs.h config.h
fcyc.o: fcyc.c fcyc.h
ftimer.o: ftimer.c ftimer.h config.h
clock.o: clock.c clock.h

handin:
	cp mm.c $(HANDINDIR)/$(TEAM)-$(VERSION)-mm.c

clean:
	rm -f *~ *.o mdriver



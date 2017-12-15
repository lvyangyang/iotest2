// mem_read.cpp : 定义控制台应用程序的入口点。
//
#include "mem.h"



void init_loop(iter_t iterations, void *cookie)
{
	state_t *state = (state_t *)cookie;

	if (iterations) return;

//	state->buf = (TYPE *)VirtualAlloc(NULL, state->nbytes, MEM_COMMIT, PAGE_READWRITE);

	state->buf=ExAllocatePoolWithTag(NonPagedPoolBaseMustSucceed, state->nbytes,"tag1");
/*	 ZwAllocateVirtualMemory(
		NtCurrentProcess(),
	    state->buf,
		NULL,
	   &(state->nbytes),
		 MEM_COMMIT,
		 PAGE_READWRITE
	);
*/
	state->buf2_orig = NULL;
	state->lastone = (TYPE*)state->buf - 1;
	state->lastone = (TYPE*)((char *)state->buf + state->nbytes - 512);
	state->N = state->nbytes;

	if (!state->buf) {
		KdPrint(("malloc"));
		return;
	}
	memset((void*)state->buf, 0, state->nbytes);

	if (state->need_buf2 == 1) {
	//	state->buf2_orig = state->buf2 = (TYPE *)malloc(state->nbytes + 2048);
		state->buf2 = ExAllocatePoolWithTag(NonPagedPoolBaseMustSucceed, state->nbytes, "tag2");
/*		ZwAllocateVirtualMemory(
			NtCurrentProcess(),
			state->buf2,
			NULL,
			&(state->nbytes),
			MEM_COMMIT,
			PAGE_READWRITE
		);
*/
		if (!state->buf2) {
			KdPrint(("malloc"));
			return;
		}
		state->buf2_orig = state->buf2;
		/* default is to have stuff unaligned wrt each other */
		/* XXX - this is not well tested or thought out */
		if (state->aligned) {
			char	*tmp = (char *)state->buf2;

			tmp += 2048 - 128;
			state->buf2 = (TYPE *)tmp;
		}
	}
}

#pragma LOCKEDCODE
void rd(iter_t iterations, void *cookie)
{
	state_t *state = (state_t *)cookie;
	register TYPE *lastone = state->lastone;
	register int sum = 0;

	while (iterations-- > 0) {
		register TYPE *p = state->buf;
		while (p <= lastone) {
			sum +=
#define	DOIT(i)	p[i]+
				DOIT(0) DOIT(4) DOIT(8) DOIT(12) DOIT(16) DOIT(20) DOIT(24)
				DOIT(28) DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52)
				DOIT(56) DOIT(60) DOIT(64) DOIT(68) DOIT(72) DOIT(76)
				DOIT(80) DOIT(84) DOIT(88) DOIT(92) DOIT(96) DOIT(100)
				DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120)
				p[124];
			p += 128;
		}
	}
	//	use_int(sum);
}
#undef	DOIT

#pragma LOCKEDCODE
void wr(iter_t iterations, void *cookie)
{
	state_t *state = (state_t *)cookie;
	register TYPE *lastone = state->lastone;

	while (iterations-- > 0) {
		register TYPE *p = state->buf;
		while (p <= lastone) {
#define	DOIT(i)	p[i] = 1;
			DOIT(0) DOIT(4) DOIT(8) DOIT(12) DOIT(16) DOIT(20) DOIT(24)
				DOIT(28) DOIT(32) DOIT(36) DOIT(40) DOIT(44) DOIT(48) DOIT(52)
				DOIT(56) DOIT(60) DOIT(64) DOIT(68) DOIT(72) DOIT(76)
				DOIT(80) DOIT(84) DOIT(88) DOIT(92) DOIT(96) DOIT(100)
				DOIT(104) DOIT(108) DOIT(112) DOIT(116) DOIT(120) DOIT(124);
			p += 128;
		}
	}
}
#undef	DOIT


void cleanup(iter_t iterations, void *cookie)
{
	state_t *state = (state_t *)cookie;


//	free(state->buf);
	ExFreePoolWithTag(state->buf,"tag1");

	if (state->buf2_orig) 
	//free(state->buf2_orig);
	ExFreePoolWithTag(state->buf2, "tag2");
}



/*int main()
{
	CStopWatch timer;
	state_t mem_state;
	mem_state.nbytes = 1024 * 1024 * 512;
	mem_state.need_buf2 = 0;
	init_loop(0, &mem_state);
	iter_t  iter = 1;
	double duration = 0;
	SetThreadAffinityMask(GetCurrentThread(), 1);
	while (TRUE)
	{


		timer.start();
		wr(iter, &mem_state);
		timer.stop();
		duration = timer.getElapsedTime();
		iter = iter * 2;
		if (duration > 0.155)
			break;
	}
	printf("mem read speed %f\n", (double)iter*mem_state.nbytes / (1024 * 1024 * 1024) / duration);
	Sleep(3000);
	return 0;
}
*/

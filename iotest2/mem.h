#pragma once
#include <fltKernel.h>  
#include <wdf.h>  
#include <stdlib.h>
#include <Ntddk.h>
#include <wdfdriver.h>  
#include <ntstrsafe.h>
#include <wdmsec.h>
#include <wdfrequest.h> 
#include <wdm.h>


#define PAGEDCODE code_seg("PAGE");  
#define LOCKEDCODE code_seg();  
#define INITCODE code_seg("INIT");  

#define PAGEDDATA code_seg("PAGE");  
#define LOCKEDDATA code_seg();  
#define INITDATA code_seg("INIT");

#define TYPE int
typedef unsigned long iter_t;
typedef struct _state {
	double	overhead;
	size_t	nbytes;
	int	need_buf2;
	int	aligned;
	TYPE	*buf;
	TYPE	*buf2;
	TYPE	*buf2_orig;
	TYPE	*lastone;
	size_t	N;
} state_t;



void init_loop(iter_t iterations, void *cookie);
void rd(iter_t iterations, void *cookie);
void wr(iter_t iterations, void *cookie);
void cleanup(iter_t iterations, void *cookie);



#include "asm.h"

void (*jit_func) (unsigned int *from, unsigned int *to, float f);

unsigned int jit_func_start[] = { 

	0x9421FFE0,	//	stwu		r1,-32(r1)
	0x93E1001C,	//	stw		r31,28(r1)			// save R31
	0x7C3F0B78,	//	mr      	r31,r1
	0x907F0008,	//	stw		r3,8(r31)				// save R3
	0x909F000C	//	stw		r4,12(r31)			// save R4

};

unsigned int jit_func_end[] ={ 

	// load org R3,R4 -- No garanti -O3 has resets R4

 	0x807f0008,	//	lwz		r3,8(r31)				// load R3
 	0x809f000c,	//	lwz		r4,12(r31)			// load R4

	// exit

	0x81610000,	//	lwz		r11,0(r1)
	0x83EBFFFC,	//	lwz		r31,-4(r11)
	0x7D615B78,	//	mr		r1,r11
	0x4E800020 	//	blr

};

unsigned int jit_func_before_loop[] = { 

 	0x39600000, 	//	li      r11,x
 	0x39800001, 	//	li      r12,1
 	0x2C0B0001,	// 	cmpwi   r11,1
// LOOP:
	0x41800000	// 	blt-    <pass>
};

unsigned int jit_func_after_loop[] = { 

 	0x30AC0001,	// 	addic   r5,r12,1		// 4
 	0x7CAC0734,	// 	extsh   r12,r5			// 8
 	0x7C0B6000,	//	cmpw    r11,r12		// 12
	0x4BFFFFFF	// 	b       <loop>			// 16		= 0x10

// PASS:
};


ULONG size_read_x_muls_y_write( int regs, BOOL is_first )
{
	// 4 size * 4 bytes * X regs
	return (regs * 4 * 4) + ( is_first ? 2 * 4 : 0 );
}

void set_jit_loop(int loops, int code_size)
{
	int jump_to_pass = code_size + sizeof(jit_func_after_loop) +4;
	int jump_to_loop = -(code_size + sizeof(jit_func_after_loop));

	jit_func_before_loop[0]	=	0x39600000 | loops;
	jit_func_before_loop[3]	=	0x41800000 | ( jump_to_pass ) ;
	jit_func_after_loop[3]	=	0x4B800000 | ( 0x7FFFFF & jump_to_loop );
}


ULONG  jit_insert_loop_size( ULONG nsize, ULONG regs , BOOL is_last, ULONG max_in_loop  )
{
	int n;
	unsigned int msize =size_read_x_muls_y_write(  regs, FALSE ) ;

	ULONG size = 0;

	int in_cache = max_in_loop < nsize ? max_in_loop : nsize;
	int num_loops_needed = nsize / in_cache;
	int dont_fith = nsize - (num_loops_needed * in_cache);

	if (dont_fith == 0)	// no loops needed.
	{
		num_loops_needed = 0;
		in_cache = 0;
		dont_fith = nsize;
	}

	if (num_loops_needed>0)
	{
		size += sizeof(jit_func_before_loop) + sizeof(jit_func_before_loop) + (in_cache * msize);
	}

	size += (dont_fith*msize) + (is_last == TRUE ? 2 * 4 : 0 );	// load from stack, save to mem.

	return size;
}

char  *jit_insert_loop( char *jit_cache_ptr, ULONG nsize, ULONG regs, BOOL is_last, ULONG max_in_loop  )
{
	int n;
	unsigned int msize =size_read_x_muls_y_write(  regs, FALSE );

	ULONG size = 0;

	int in_cache = max_in_loop < nsize ? max_in_loop : nsize;
	int num_loops_needed = nsize / in_cache;
	int dont_fith = nsize - (num_loops_needed * in_cache);

	if (dont_fith == 0)	// no loops needed.
	{
		num_loops_needed = 0;
		in_cache = 0;
		dont_fith = nsize;
	}

	if (jit_cache_ptr)
	{
		if (num_loops_needed>0)
		{
			set_jit_loop( num_loops_needed, msize *  in_cache );

			CopyMem( jit_func_before_loop, jit_cache_ptr, sizeof(jit_func_before_loop) ); 
			jit_cache_ptr += sizeof(jit_func_before_loop);

			for (n=0;n<in_cache;n++)
			{
				jit_cache_ptr += read_x_muls_y_write( (ULONG *)  jit_cache_ptr, regs, FALSE );
			}

			CopyMem( jit_func_after_loop, jit_cache_ptr, sizeof(jit_func_after_loop) ); 
			jit_cache_ptr += sizeof(jit_func_before_loop);
		}

		for (n=0;n<dont_fith;n++)
		{
			if (n == dont_fith-1) 
			{
				jit_cache_ptr += read_x_muls_y_write( (ULONG *) jit_cache_ptr, regs, is_last );
			}
			else
			{
				jit_cache_ptr += read_x_muls_y_write( (ULONG *) jit_cache_ptr, regs, FALSE );
			}
		}
	}

	return jit_cache_ptr;
}


ULONG *idea( int loops, int max_regs, ULONG *min_regs_used, ULONG *max_regs_used, ULONG max_in_loop )
{
	char *jit_cache ;			// most be ptr of bytes.
	char *jit_cache_ptr;

	int max_block_loops = loops < 31 ? loops : 31;
	ULONG sizes[32];
	int regs;
	int cloops = loops;
	ULONG size,diff;

//	printf("REGS\t\tLOOPS\t\tSIZE\t\tSIZE with loop\n");

	bzero(sizes,sizeof(sizes));

	for (regs=max_regs;regs>0;regs--)
	{
		sizes[regs-1] = cloops / regs;
		cloops -= (sizes[regs-1] * regs);
	}

	size = sizeof(jit_func_start) + sizeof(jit_func_end);

	*max_regs_used = 0;
	*min_regs_used = 32;

	for (regs=31;regs>0;regs--)
	{
		if ((sizes[regs-1]>0) && (regs<*min_regs_used)) *min_regs_used = regs;
		if ((sizes[regs-1]>0) && (regs>*max_regs_used)) *max_regs_used = regs;
	}

	for (regs=max_regs;regs>0;regs--)
	{
		if (sizes[regs-1]>0)
		{
			diff = jit_insert_loop_size	( sizes[regs-1], regs, regs == *min_regs_used ? TRUE : FALSE , max_in_loop );
/*
			printf("R%d\t\t%d\t\t%d\t\t%d\n",
				regs, sizes[regs-1] *regs, 
				sizes[regs-1] *size_read_x_muls_y_write(  regs, regs == *min_regs_used ? TRUE : FALSE  ), 
				diff );
*/
			size += diff;
		}
	}

//	printf("Tottal size needed: %d\n",size);

	jit_cache = AllocVecTags( size , AVT_Type, MEMF_EXECUTABLE ,
			AVT_Contiguous, TRUE, 
			AVT_PhysicalAlignment,TRUE,
			TAG_END);

//	printf ("JIT cache from\t\t%08x TO %08x\n", jit_cache, jit_cache + size );

	if (jit_cache)
	{
		jit_cache_ptr = jit_cache;

//		printf("0x%08X - func() {\n",jit_cache_ptr);

		CopyMem( jit_func_start, jit_cache_ptr, sizeof(jit_func_start) ); 
		jit_cache_ptr += sizeof(jit_func_start);



		for (regs=max_regs;regs>0;regs--)
		{
			if (sizes[regs-1]>0)
			{
//		printf("%08X\n", sizes[regs-1] * regs);
//				printf("0x%08X -      float_to_int( %d,%d );  // floats %d\n", jit_cache_ptr,  regs,sizes[regs-1] , sizes[regs-1] * regs );

				diff = (int) jit_cache_ptr;

				jit_cache_ptr = jit_insert_loop	( jit_cache_ptr , sizes[regs-1], regs, regs == *min_regs_used ? TRUE : FALSE, max_in_loop );

//				printf("        Size: %d\n", (int) jit_cache_ptr - diff );

			}
		}

//		printf("0x%08X -         // just before the end\n",jit_cache_ptr);

		CopyMem( jit_func_end, jit_cache_ptr, sizeof(jit_func_end) ); 
		jit_cache_ptr += sizeof(jit_func_end);

//		printf("0x%08X - }\n",jit_cache_ptr);

		CacheClearE(jit_cache,size,CACRF_ClearI);
	}
	
	return (ULONG *) jit_cache;
}


ULONG read_x_muls_y_write( ULONG *ptr, int regs, BOOL is_first )
{
	int cnt = 0;
	int min_reg = 1;
	int max_reg = min_reg + regs-1;
	int RT;

	for (RT=min_reg;RT<=max_reg;RT++)
	{
		ptr[ cnt++ ] = m_lfsu( dRT ,-4,3);
	}

	for (RT=min_reg;RT<=max_reg;RT++)
	{
		ptr[ cnt++ ] = m_muls( dRT, dRT ,1);
		ptr[ cnt++ ] = m_fctiwz( dRT,dRT);
	}

	for (RT=min_reg;RT<=max_reg;RT++)
	{
//		ptr[ cnt++ ] = m_fctiwz( dRT,dRT);
	}

	if (is_first)
	{
		for (RT=min_reg;RT<max_reg;RT++)
		{
			ptr[ cnt++ ] = m_stfdu(dRT, -4 ,4);
		}

		RT = max_reg;

		ptr[ cnt++ ] = m_stfd( dRT,16,31);
		ptr[ cnt++ ] = m_lwz(0,20,31);
		ptr[ cnt++ ] = m_stwu(0,0,4);
	}
	else
	{
		for (RT=min_reg;RT<=max_reg;RT++)
		{
			ptr[ cnt++ ] = m_stfdu(dRT, -4 ,4);
		}
	}

	return cnt *4 ;
}


void jit_free(void *jit_cache)
{
	if (jit_cache)
	{
		FreeVec(jit_cache);
		jit_cache = NULL;
	}
}



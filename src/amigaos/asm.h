
#include <proto/exec.h>
#include <proto/dos.h>

extern void (*jit_func) (unsigned int *from, unsigned int *to, float f);

extern unsigned int jit_func_start[];
extern unsigned int jit_func_end[];

extern unsigned int jit_func_before_loop[];
extern unsigned int jit_func_after_loop[];


#define m_lfsu(a,b,c)	( 0xC4000000 | (a<<21) | ( b & 0xFFFF) | (c<<16) )

#define m_muls(a,b,c)	( 0xEC000032 | (a<<21) | (b<<16) | (c<<6) )
#define m_fctiwz(a,b)	( 0xFC00001E | (a<<21) | (b<<11) )

#define m_stfd(a,b,c)	( 0xD8000000 | (a<<21) | ( b & 0xFFFF) | (c<<16) )
#define m_stfdu(a,b,c)	( 0xDC000000 | (a<<21) | ( b & 0xFFFF) | (c<<16) )

#define m_lwz(a,b,c)	( 0x80000000 | (a<<21) | ( b & 0xFFFF) | (c<<16) )

#define m_stw(a,b,c)	( 0x90000000 | (a<<21) | ( b & 0xFFFF) | (c<<16) )
#define m_stwu(a,b,c)	( 0x94000000 | (a<<21) | ( b & 0xFFFF) | (c<<16) )

#define m_addi(a,b,c)	( 0x80000000 | (a<<21) | ( c & 0xFFFF) | (b<<16) )

#define dRT (RT == 1 ? 0 : RT),(RT == 1 ? 0 : RT)

extern ULONG read_x_muls_y_write( ULONG *ptr, int regs, BOOL is_first );
extern ULONG size_read_x_muls_y_write( int regs, BOOL is_first );
extern void set_jit_loop(int loops, int code_size);
extern ULONG  jit_insert_loop_size( ULONG nsize, ULONG regs , BOOL is_last, ULONG max_in_loop  );
extern char  *jit_insert_loop( char *jit_cache_ptr, ULONG nsize, ULONG regs, BOOL is_last, ULONG max_in_loop  );
extern ULONG *idea( int loops, int max_regs, ULONG *min_regs_used, ULONG *max_regs_used, ULONG max_in_loop );
extern ULONG read_x_muls_y_write( ULONG *ptr, int regs, BOOL is_first );
extern void jit_free(void *jit_cache);


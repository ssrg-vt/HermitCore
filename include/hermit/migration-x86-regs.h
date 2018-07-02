#ifndef MIGRATION_X86_REG_H
#define MIGRATION_X86_REG_H

/* Code copied from the popcorn stack transformation library */

#define GET_REG( var, reg, size ) asm volatile("mov"size" %%"reg", %0" : "=m" (var) )
#define GET_REG64( var, reg ) GET_REG( var, reg, "q" )

#define SET_REG( var, reg, size ) asm volatile("mov"size" %0, %%"reg : : "m" (var) : reg )
#define SET_REG64( var, reg ) SET_REG( var, reg, "q" )

/* General-purpose x86-64 registers */
#define GET_RAX( var ) GET_REG64( var, "rax" )
#define GET_RBX( var ) GET_REG64( var, "rbx" )
#define GET_RCX( var ) GET_REG64( var, "rcx" )
#define GET_RDX( var ) GET_REG64( var, "rdx" )
#define GET_RBP( var ) GET_REG64( var, "rbp" )
#define GET_RSI( var ) GET_REG64( var, "rsi" )
#define GET_RDI( var ) GET_REG64( var, "rdi" )
#define GET_RSP( var ) GET_REG64( var, "rsp" )
#define GET_R8( var ) GET_REG64( var, "r8"  )
#define GET_R9( var ) GET_REG64( var, "r9"  )
#define GET_R10( var ) GET_REG64( var, "r10" )
#define GET_R11( var ) GET_REG64( var, "r11" )
#define GET_R12( var ) GET_REG64( var, "r12" )
#define GET_R13( var ) GET_REG64( var, "r13" )
#define GET_R14( var ) GET_REG64( var, "r14" )
#define GET_R15( var ) GET_REG64( var, "r15" )

#define SET_RAX( var ) SET_REG64( var, "rax" )
#define SET_RBX( var ) SET_REG64( var, "rbx" )
#define SET_RCX( var ) SET_REG64( var, "rcx" )
#define SET_RDX( var ) SET_REG64( var, "rdx" )
#define SET_RBP( var ) SET_REG64( var, "rbp" )
#define SET_RSI( var ) SET_REG64( var, "rsi" )
#define SET_RDI( var ) SET_REG64( var, "rdi" )
#define SET_RSP( var ) SET_REG64( var, "rsp" )
#define SET_R8( var ) SET_REG64( var, "r8"  )
#define SET_R9( var ) SET_REG64( var, "r9"  )
#define SET_R10( var ) SET_REG64( var, "r10" )
#define SET_R11( var ) SET_REG64( var, "r11" )
#define SET_R12( var ) SET_REG64( var, "r12" )
#define SET_R13( var ) SET_REG64( var, "r13" )
#define SET_R14( var ) SET_REG64( var, "r14" )
#define SET_R15( var ) SET_REG64( var, "r15" )

#endif /* MIGRATION_X86_REG_H */

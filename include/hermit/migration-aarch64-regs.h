#ifndef MIGRATION_AARCH64_REGS_H
#define MIGRATION_AARCH64_REGS_H

/* Code copied from the popcorn stack transformation library */

#define GET_REG( var, reg, size ) asm volatile("str"size" "reg", %0" : "=m" (var) )
#define GET_REG64( var, reg ) GET_REG( var, reg, "" )

#define SET_REG( var, reg, size ) asm volatile("ldr"size" "reg", %0" : : "m" (var) : reg )
#define SET_REG64( var, reg ) SET_REG( var, reg, "" )

/* General-purpose aarch64 registers */
#define GET_X0( var ) GET_REG64( var, "x0" )
#define GET_X1( var ) GET_REG64( var, "x1" )
#define GET_X2( var ) GET_REG64( var, "x2" )
#define GET_X3( var ) GET_REG64( var, "x3" )
#define GET_X4( var ) GET_REG64( var, "x4" )
#define GET_X5( var ) GET_REG64( var, "x5" )
#define GET_X6( var ) GET_REG64( var, "x6" )
#define GET_X7( var ) GET_REG64( var, "x7" )
#define GET_X8( var ) GET_REG64( var, "x8" )
#define GET_X9( var ) GET_REG64( var, "x9" )
#define GET_X10( var ) GET_REG64( var, "x10" )
#define GET_X11( var ) GET_REG64( var, "x11" )
#define GET_X12( var ) GET_REG64( var, "x12" )
#define GET_X13( var ) GET_REG64( var, "x13" )
#define GET_X14( var ) GET_REG64( var, "x14" )
#define GET_X15( var ) GET_REG64( var, "x15" )
#define GET_X16( var ) GET_REG64( var, "x16" )
#define GET_X17( var ) GET_REG64( var, "x17" )
#define GET_X18( var ) GET_REG64( var, "x18" )
#define GET_X19( var ) GET_REG64( var, "x19" )
#define GET_X20( var ) GET_REG64( var, "x20" )
#define GET_X21( var ) GET_REG64( var, "x21" )
#define GET_X22( var ) GET_REG64( var, "x22" )
#define GET_X23( var ) GET_REG64( var, "x23" )
#define GET_X24( var ) GET_REG64( var, "x24" )
#define GET_X25( var ) GET_REG64( var, "x25" )
#define GET_X26( var ) GET_REG64( var, "x26" )
#define GET_X27( var ) GET_REG64( var, "x27" )
#define GET_X28( var ) GET_REG64( var, "x28" )
#define GET_X29( var ) GET_REG64( var, "x29" )
#define GET_X30( var ) GET_REG64( var, "x30" )

#define SET_X0( var ) SET_REG64( var, "x0" )
#define SET_X1( var ) SET_REG64( var, "x1" )
#define SET_X2( var ) SET_REG64( var, "x2" )
#define SET_X3( var ) SET_REG64( var, "x3" )
#define SET_X4( var ) SET_REG64( var, "x4" )
#define SET_X5( var ) SET_REG64( var, "x5" )
#define SET_X6( var ) SET_REG64( var, "x6" )
#define SET_X7( var ) SET_REG64( var, "x7" )
#define SET_X8( var ) SET_REG64( var, "x8" )
#define SET_X9( var ) SET_REG64( var, "x9" )
#define SET_X10( var ) SET_REG64( var, "x10" )
#define SET_X11( var ) SET_REG64( var, "x11" )
#define SET_X12( var ) SET_REG64( var, "x12" )
#define SET_X13( var ) SET_REG64( var, "x13" )
#define SET_X14( var ) SET_REG64( var, "x14" )
#define SET_X15( var ) SET_REG64( var, "x15" )
#define SET_X16( var ) SET_REG64( var, "x16" )
#define SET_X17( var ) SET_REG64( var, "x17" )
#define SET_X18( var ) SET_REG64( var, "x18" )
#define SET_X19( var ) SET_REG64( var, "x19" )
#define SET_X20( var ) SET_REG64( var, "x20" )
#define SET_X21( var ) SET_REG64( var, "x21" )
#define SET_X22( var ) SET_REG64( var, "x22" )
#define SET_X23( var ) SET_REG64( var, "x23" )
#define SET_X24( var ) SET_REG64( var, "x24" )
#define SET_X25( var ) SET_REG64( var, "x25" )
#define SET_X26( var ) SET_REG64( var, "x26" )
#define SET_X27( var ) SET_REG64( var, "x27" )
#define SET_X28( var ) SET_REG64( var, "x28" )
#define SET_X29( var ) SET_REG64( var, "x29" )
#define SET_X30( var ) SET_REG64( var, "x30" )

/*
 * The only way to set the PC is through control flow operations.
 */
#define SET_PC_IMM( val ) asm volatile("b %0" : : "i" (val) )
#define SET_PC_REG( val ) asm volatile("br %0" : : "r" (val) )

/*
 * The stack pointer is a little weird because you can't read it directly into/
 * write it directly from memory.  Move it into another register which can be
 * saved in memory.
 */
#define GET_SP( var ) asm volatile("mov x15, sp; str x15, %0" : "=m" (var) : : "x15")
#define SET_SP( var ) asm volatile("ldr x15, %0; mov sp, x15" : : "m" (var) : "x15")

#endif /* MIGRATION_AARCH64_REGS_H */

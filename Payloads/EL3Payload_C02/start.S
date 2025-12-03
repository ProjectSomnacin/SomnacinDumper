.globl _start
.thumb
_start:
  // First reload the stack pointer to point to some empty area in SRAM
  ldr r0, =0x54000
  mov sp, r0

  // Jump to the main function
  // (First parameter is our postcode function)
  adr r0, postcode+1
  bl c_main
  b .

postcode:
  push {r4,lr}
  mov r2, r0
  ldr r4, =0x59bd
  ldr r0, =0xFC000080
  mov r1, 0xFFFD
  mov r3, 4
  blx r4
  ldr r4, =0x1000000
99:
  cmp r4, #0
  beq 98f
  sub r4, r4, #1
  b 99b
98:
  pop {r4,pc}

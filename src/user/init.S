#include <core/syscallno.h>
#include <sys/syscall.h>

.global icode
.global ispin
.global eicode

icode:
    mov     x8, #SYS_execve
    adr     x0, init
    mov     x1, #0
    mov     x2, #0
    svc     #0
	mov     x8, #SYS_myexit
    svc     #0

ispin:
    mov		x8, #SYS_myyield
    svc		#0
    b       ispin

exit:
    mov     x8, #SYS_myexit
    svc     #0
    b exit

init:
    .string "/init\0"

.align 4
eicode:

PROG= chericat
MAN=  chericat.1
SRCS= src/cap_capture.c src/caps_syms_view.c src/chericat.c src/common.c src/db_process.c src/elf_utils.c src/mem_scan.c src/ptrace_utils.c src/rtld_linkmap_scan.c src/vm_caps_view.c

PREFIX?=     /usr/local
SRC_BASE?=   /usr/src
ARCH?=       aarch64
LDADD+=      -lelf -lprocstat -lsqlite3 -lxo 

.if !defined(LOCALBASE)
CFLAGS+=     -I${PREFIX}/include -I./includes -I${SRC_BASE}/libexec/rtld-elf -I${SRC_BASE}/libexec/rtld-elf/${ARCH} -L${PREFIX}/lib -DIN_RTLD -DCHERI_LIB_C18N
.endif

.include <bsd.prog.mk>

PROG= chericat
MAN=  chericat.1
.PATH: ${.CURDIR}/src
SRCS= cap_capture.c caps_syms_view.c chericat.c common.c db_process.c elf_utils.c mem_scan.c ptrace_utils.c rtld_linkmap_scan.c vm_caps_view.c

PREFIX?=     /usr/local
SRC_BASE?=   /usr/src
ARCH?=       aarch64
LDADD+=      -lelf -lprocstat -lsqlite3 -lxo 

.if !defined(LOCALBASE)
CFLAGS+=     -I${PREFIX}/include -I./includes -I${SRC_BASE}/libexec/rtld-elf -I${SRC_BASE}/libexec/rtld-elf/${ARCH} -L${PREFIX}/lib -DIN_RTLD -DCHERI_LIB_C18N
.endif

.include <bsd.prog.mk>

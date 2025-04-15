/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Capabilities Limited
 *
 * This software was developed by Capabilities Limited under Innovate UK
 * project 10027440, "Developing and Evaluating an Open-Source Desktop for Arm
 * Morello".
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef RTLD_LINKMAP_SCAN_H_
#define RTLD_LINKMAP_SCAN_H_

#include <sqlite3.h>
#include <stdbool.h>

#include <sys/link_elf.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <libprocstat.h>

typedef struct struct_compart_data {
    int id;
    int names_array_size;
    char **names_array;
    char *path;
    Elf_Addr start_addr;
    Elf_Addr end_addr;
    bool is_default;
} compart_data;

typedef struct struct_compart_data_list {
	compart_data data;
	struct struct_compart_data_list *next;
} compart_data_list;

struct r_debug get_r_debug(int pid, struct procstat *psp, struct kinfo_proc *kipp);
void getprocs_with_procstat_sysctl(sqlite3 *db, int pid);
compart_data_list *scan_rtld_linkmap(int pid, sqlite3 *db, struct r_debug target_debug);
char **scan_r_comparts(int pid, sqlite3 *db, struct r_debug target_debug);

#endif //RTLD_LINKMAP_SCAN_H_

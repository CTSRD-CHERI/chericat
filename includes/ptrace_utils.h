#ifndef PTRACE_UTILS_H_
#define PTRACE_UTILS_H_

void ptrace_attach(int pid);
void ptrace_detach(int pid);
void read_data(int pid, void *addr, void* vptr, int len);
void read_lwps(char *arg_pid); 
void get_vmmap_from_procstat(char *pid);

#endif //PTRACE_UTILS_H_

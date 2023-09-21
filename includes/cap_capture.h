#ifndef CAP_CAPTURE_H_
#define CAP_CAPTURE_H_

typedef struct cap_info_struct {
	uintcap_t addr_of_cap;
	uintcap_t addr_at_which_cap_references;
	char *perms;
} Cap_info_struct;

typedef struct vm_cap_info_struct {
	u_long start_addr;
	u_long end_addr;
	char *path;
	int cap_count;
	Cap_info_struct **cap_info;
} Vm_cap_info_struct;

void get_capability(int pid, void* addr, int current_cap_count, Vm_cap_info_struct *vm_cap_info, char **query_vals);

void get_tags(int pid, u_long start, Vm_cap_info_struct *vm_cap_info);

void print_vm_block(Vm_cap_info_struct vm_cap_info, xo_handle_t *xop); 

#endif //CAP_CAPTURE_H_

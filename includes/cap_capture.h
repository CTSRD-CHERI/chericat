#ifndef CAP_CAPTURE_H_
#define CAP_CAPTURE_H_

typedef struct cap_info_struct {
	uintcap_t cap_loc_addr;
	char *cap_path;
	uintcap_t cap_addr;
	char *perms;
	char *base;
	char *top;
} Cap_info_struct;

typedef struct vm_cap_info_struct {
	u_long start_addr;
	u_long end_addr;
	char *path;
	Cap_info_struct *cap_info;
	int cap_count;
} Vm_cap_info_struct;

void get_capability(int pid, void* addr, int current_cap_count, char *path, char **query_vals);
int get_tags(sqlite3 *db, int pid, u_long start, char *path);

#endif //CAP_CAPTURE_H_

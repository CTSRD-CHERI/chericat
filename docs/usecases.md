## Capabilities Stats
It is not easy to find out the number and distribution of the capabilities during runtime without instrumenting the existing CHERI code to extract the information. Having an external tool that can scan the capability tags would save a great deal of effort. This would provide useful stats for other use cases such as validation as described below.

The overview summary table similar to procstat(1) output is a useful first step to display the stats to users, to show some basic process properties, such as capability counts per memory mapping (including that some mappings contain no capabilities). This could be used to demonstrate that capabilities are in use at all. Also, simply being able to print all capabilities held in a memory mapping (or in all mappings), and what they point to (e.g., into a mapping of what file, anonymous memory, etc) would be useful for debugging and exploration.

The future plan is to create separate groups of commands for users to drill down into more detailed stats, for example options that allow users to analyse simple dynamic properties – what memory mappings point (grant access to) what others. In the context of the compartmentalisation, one might be interested to find out, although there are sealed capabilities out of the sandbox, there are no unsealed ones, which should be quite obvious if true.

## Overview of capabilities leakage
Pointers to, e.g. a struct, are being passed around, would be useful to show how many of those capabilities have been copied across compartments. We will need per compartment statistics, drill down on each incoming and outgoing references.

It would be useful to find out how the relationship between compartments evolve over time.

## Validation of capability changes during runtime
A possible optional activity (but perhaps desirable) is to have the ability to check specific kinds of properties; e.g., that sandboxes have no outbound pointers that are unsealed, or that revocation has taken place.
Chericat fork and exec mode - maybe remote control? To have chericat to be always there but can be controlled to take snapshots at various points.

## Ownership of highly privileged capabilities
High privileged capabilities such as those that have vmem permissions:

    Cheri_perm_sw_vmem (cheri_perm_sw1)

should not be leaking out of malloc (not out of libc, unless for specific programmed mmap users). They are part of the mmap() function family and have the ability change the properties of memory, and therefore it is undesirable to have such privileged capabilities leaked to unintended compartments. 

Using Chericat, for a given set of (high privileged) permissions we can query for the capabilities and find out which compartments they are located in.

## Kernel and Threads View


Each thread has its own stack and registers set, Chericat can make use of the kvm_getprocs() call to obtain data. The data can create reachability analysis from each thread.

Current thinking is that the data can be stored in a separate db table.

Chericat can also capture capabilities in the kernel space and registers content within each kernel thread. The data can be used to verify if there are undesirable leakage across compartments. 

Again, we could look for capabilities that have high privileged permissions (i.e. have access to all the memory/address space) and find out where are they located within the kernel space.


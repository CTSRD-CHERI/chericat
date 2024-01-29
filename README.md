# chericat
<p align="center">
<img src="chericat.jpg" alt="chericat" width="150"/>
</p>

A Capability Analysis Tool for CHERI

The program “chericat” is a standalone tool that runs on CheriBSD. It can be used by attaching to a running process to produce capability-related data, which is stored in a local SQL database. The data can then be queried to retrieve information regarding the capabilities created at that snapshot.

When the -p option is used, chericat uses the procstat utility functions to take a snapshop of the running process with the provided <pid> and extract the mmap data. At the same time chericat obtains the ELF data using the ELF Toolchain library. The data is then written to either a SQLite database if a valid database name is provided via the -f option, or to memory just to output the data without storing permanently.

To display formatted data on the console, the -v or -c options can be used. The -v option shows a table with twelve colunms:

|Name|Description|
|---|---|
|START|The address of the start of the scanned vm block|
|END|The address of the end of the scanned vm block|
|PRT|Protection flags of the vm block, r=read, w=write, x=executable, R=can read capabilities, W=can write capabilities, -=not allowed
|ro|Number of read-only capabilities found in this vm block|
|rw|Number of read-write capabilities found in this vm block|
|rx|Number of read-and-executable enabled capabilities found in this vm block|
|rwx|Number of read-write-executable enabled capabilities found in this vm block|
|TOTAL|Total number of capabilities found in this vm block|
|DENSITY|Percentage of capabilities found |
|FLAGS|Vm mapping flags|
|TP|Vm object type|
|PATH|Library name/path where the scanned vm block belongs|

To display the obtained symbols for the capabilities within a specific library, use the -c option:

|Name|Description|
|---|---|
|CAP_LOC|Address of the capabilities|
|CAP_LOC_SYM (TYPE)|Name of the symbol (if found on ELF) for the capability, the type could be NOTYPE if the information is not available, OBJECT if the capability is an object type, or FUNC, if the capability is a function type|
|CAP_INFO|Referenced address stored in this capability, the permissions, end address of the capability (which in turns shows how big it is)|
|CAP_SYM (TYPE)|Name of the symbol (if found on ELF) of the referenced capability|

```
Usage: chericat [-d <debug level>] [-f <database name>] [-p <pid>] [-v] [-c <binary name>]
     debug level - 0 = No output; 1 = INFO; 2 = VERBOSE; 3 = TROUBLESHOOT
     pid - pid of the target process for a snapshot of caps info
     database name - name of the database to store data captured by chericat
Options:
     -d Determine the level of debugging messages to be printed. If omitted, the default is INFO level
     -f Provide the database name to capture the data collected by chericat. If omitted, an in-memory db is used
     -p Scan the mapped memory and persist the caps data to a database
     -v Show virtual summary info of capabilities in the target process, arranged in mmap order
     -c Show capabilities with corresponding symbols located in the provided binary
```

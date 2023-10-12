# chericat
<p align="center">
<img src="chericat.jpg" alt="chericat" width="150"/>
</p>

A Capability Analysis Tool for CHERI

The program “chericat” is a standalone tool that runs on CheriBSD. It can be used by either attaching to a running process or parsing a coredump to produce capability-related data, whic is stored in a local SQL database. The data can then be queried to retrieve information regarding the capabilities created at that snapshot.

```
Usage: chericat [-g <debug level>] [-d <database name>] [-p <pid>] [-v] [-c <binary name>]
     debug level - 0 = No output; 1 = INFO; 2 = VERBOSE; 3 = TROUBLESHOOT
     pid - pid of the target process for a snapshot of caps info
     database name - name of the database to store data captured by chericat
Options:
     -g Determine the level of debugging messages to be printed. If omitted, the default is INFO level
     -d Provide the database name to capture the data collected by chericat. If omitted, an in-memory db is used
     -p Scan the mapped memory and persist the caps data to a database
     -v Show virtual summary info of capabilities in the target process, arranged in mmap order
     -c Show capabilities with corresponding symbols located in the provided binary
```

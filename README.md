# chericat
<p align="center">
<img src="chericat.jpg" alt="chericat" width="150"/>
</p>

A Capability Analysis Tool for CHERI

The program “chericat” is a standalone tool that runs on CheriBSD. It can be used by either attaching to a running process or parsing a coredump to produce capability-related data, whic is stored in a local SQL database. The data can then be queried to retrieve information regarding the capabilities created at that snapshot.

Usage: chericat [-d  &lt;database name&gt;][-s <pid>][-v][-n  &lt;binary name&gt;] [-m]\
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;pid - pid of the target process for a snapshot of caps info\
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;database name - name of the database to store data captured by chericat\
\
Options:\
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;-s Scan the mapped memory and persist the caps data to a database\
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;-v Show summary info of capabilities in the target process, arranged in mmap order\
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(WIP)-n Show capabilities in the loaded binary <binary name>\
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(WIP)-m Show capabilities with corresponding symbols\






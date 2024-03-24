# Chericat Python
<p align="center">
<img src="chericat.jpg" alt="chericat" width="150"/>
</p>

A set of python code to display Chericat capabilities related data on graphs

Packages required to run the programs:

|Name|Tested Version|
|---|---|
|Courier Prime|g20180831_1|
|Graphviz|7.1.0_7|
|Python|3.9.17|
|Sqlite3|3.42.0|

The main program to run is "chericat_graphs.py", which can be invoked by running 

```console
$ python3.9 chericat_graphs.py
usage: chericat_graphs [-h] -d D [-g] [-r R] [-c C C] [-l]
chericat_graphs: error: the following arguments are required: -d
```

Without specifying any parameters to chericat_graphs.py it would complain about the missing of the compulsory argument, -d, which is the SQLite database containing the chericat capabilities data for the graphs.

To show all the options available and their brief description, use the --help, -h option:

```console
$ python3.9 chericat_graphs.py --help
usage: chericat_graphs [-h] -d D [-g] [-r R] [-c C C] [-l]

optional arguments:
  -h, --help  show this help message and exit
  -d D        The database to use for the queries
  -g          Generate full capability relationship in mmap graph
  -r R        Executes the SQL query on the provided db
  -c C C      Show capabilities between two loaded libraries <libname 1>
              <libname 2>
  -l          Show compartments graph
```

## Capabilitiy Relationship Overview Graph
The option -g can be used to generate a directed graph 

import sys
sys.path.append('python')

from utils import db_utils
import gdb
import sqlite3

db = 'pp.db'

def _iterate_blocks(block):
    curr = block
    while curr != 0:
        yield curr
        curr = curr["next"]

def get_heap_blocks():
    b0 = gdb.lookup_symbol("b0")[0].value()
    return _iterate_blocks(b0["blocks"])

def find_heap_in_vm():
    lib_start_q = "SELECT start_addr FROM vm"
    lib_end_q = "SELECT end_addr FROM vm"
    lib_start_addrs = db_utils.run_sql_query(db, lib_start_q)
    #lib_end_addrs = db_utils.run_sql_query('pp.db', lib_end_q)

    for block_addr in get_heap_blocks():
        for addr_idx in range(len(lib_start_addrs)):
            lib_start_addr = lib_start_addrs[addr_idx][0]
            lib_end_q = "SELECT end_addr FROM vm WHERE start_addr=\""+lib_start_addr+"\""
            lib_end_addr = db_utils.run_sql_query(db, lib_end_q)[0][0]
            if (block_addr.format_string() >= lib_start_addr and \
                    block_addr.format_string() <= lib_end_addr):
                    print(block_addr.format_string() + " is >= " + lib_start_addr + " and <= " + lib_end_addr)
                    update_q = "UPDATE vm SET mmap_path='heap block' WHERE start_addr=\""+lib_start_addr+"\" AND end_addr=\""+lib_end_addr+"\""
                    result = db_utils.run_sql_query(db, update_q)
                    print(result)

find_heap_in_vm()

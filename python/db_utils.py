import graphviz
import json
import sqlite3

def run_sql_query(db, query):
    conn = sqlite3.connect(db)
    cur = conn.cursor()
    cur.execute(query)
    result_data = json.dumps(cur.fetchall())
    result_json = json.loads(result_data)
    return result_json

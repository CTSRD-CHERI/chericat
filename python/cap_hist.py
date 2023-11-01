# SQLite data can be exported to a CSV using the following sqlite3 commands:
# sqlite> .mode csv
# sqlite> .output <name>.csv
# sqlite> select perms,count(*) from cap_info group by perms;
# sqlite> .output stdout

import matplotlib.pyplot as plt
import pandas as pd
import csv

def display_perms_hist(data_csv):
    perms = []
    count = []
    
    perms_count = pd.read_csv(data_csv, sep=',', header=None, names=['perms', 'count'])
    
    plt.bar(perms_count['perms'], perms_count['count'])
    plt.xlabel('Permission Combinations')
    plt.ylabel('Count')
    plt.title('Frequency of each permission combination discovered from ' + data_csv[:-4])
    plt.show()

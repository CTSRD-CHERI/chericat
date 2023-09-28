import sys, os
IPYNB_FILENAME = 'chericat_cli.ipynb'
CONFIG_FILENAME = 'chericat_cli.config'
def main(argv):
    with open(CONFIG_FILENAME, 'w') as f:
        f.write(','.join(argv))
    os.system('jupyter nbconvert --execute {:s} --to script'.format(IPYNB_FILENAME))
    return None

if __name__ == '__main__':
    main(sys.argv)

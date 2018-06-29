import os, sys
try:
    from MonetDBtesting import process
except ImportError:
    import process

def main():
    c = process.client('sql',
                       args = [os.path.join(os.getenv('TSTSRCBASE'),
                                            os.getenv('TSTDIR'),
                                            'zones2.sql')],
                       stdin = process.PIPE, stdout = process.PIPE, stderr = process.PIPE)
    out, err = c.communicate()
    sys.stdout.write(out)
    sys.stderr.write(err)

main()
main()

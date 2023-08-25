import os
import shlex
import subprocess

print('----- Python compilation tests -----')

tests: dict[str] = {}
errs: set[str] = set()

for fn in os.listdir('./tests/python'):
    idx, f = fn.split('-')
    f, kind = f.split('.')
    if kind == 'out':
        with open(os.path.join('./tests/python/','%s-%s.out'%(idx,f)),'r') as file:
            tests[idx+'-'+f] = file.read()
    
for test, out in tests.items():
    print('[COMPILE] %s'%(test),end='')
    p = subprocess.Popen('py compiler.py %s %s'%(shlex.quote(os.path.join('./tests/python/',test+'.py')),shlex.quote(os.path.join('./tests/tmp/','python-'+test+'.bsk'))),shell=True,universal_newlines=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    stdout, stderr = p.communicate()
    if p.returncode != 0:
        errs.add('cmp:'+test)
        errs.add('err:'+test)
        print('\x1b[G[\x1b[31mCOMPILE\x1b[39m] %s\x1b[K'%(test))
    else:
        print('\x1b[G[\x1b[32mCOMPILE\x1b[39m] %s\x1b[K'%(test))
    if stdout: print('\x1b[36m|\x1b[39m '+stdout.replace('\n','\n\x1b[36m|\x1b[39m '))
    if stderr: print('\x1b[31m|\x1b[39m '+stderr.replace('\n','\n\x1b[31m|\x1b[39m '))
    
print()
    
for test, out in tests.items():
    bsk_file = os.path.join('./tests/tmp/','python-'+test+'.bsk')
    if 'cmp:'+test not in errs:
        print('[RUN] %s'%(test,),end='')
        p = subprocess.Popen('./out/basik %s'%(shlex.quote(bsk_file),),shell=True,universal_newlines=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,errors='ignore')
        stdout, stderr = p.communicate()
        if p.returncode != 0:
            errs.add('run:'+test)
            errs.add('err:'+test)
            print('\x1b[G[\x1b[31mRUN\x1b[39m] %s\x1b[K'%(test))
        elif stdout != out:
            errs.add('out:'+test)
            errs.add('err:'+test)
            print('\x1b[G[\x1b[31mRUN\x1b[39m] %s\x1b[K'%(test))
            print('      \x1b[90m(Output does not match expected output)\x1b[39m')
        else:
            print('\x1b[G[\x1b[32mRUN\x1b[39m] %s\x1b[K'%(test))
        if stderr: print('\x1b[31m|\x1b[39m '+stderr.replace('\n','\n\x1b[31m|\x1b[39m '))
        os.remove(bsk_file)
    else:
        print('[\x1b[90mSKIP\x1b[39m] %s'%(test,))
    
print()

ec = sum(e.startswith('err:') for e in errs) # Error count
sc = len(tests)-ec                           # Success count
ec_cmp = sum(e.startswith('cmp:') for e in errs) # Compilation errors count
ec_run = sum(e.startswith('run:') for e in errs) # Runtime errors count
ec_out = sum(e.startswith('out:') for e in errs) # Output mismatch count
print('Success: \x1b[%dm%d\x1b[39m'%(32 if sc else 90,sc))
print('Errors:  \x1b[%dm%d\x1b[39m'%(31 if ec else 90,ec))
if ec_cmp: print('  -> \x1b[33m%d\x1b[39m Compilation error%s'%(ec_cmp,'s' if ec_cmp!=1 else ''))
if ec_run: print('  -> \x1b[33m%d\x1b[39m Runtime error%s'%(ec_run,'s' if ec_run!=1 else ''))
if ec_out: print('  -> \x1b[33m%d\x1b[39m Output mismatch%s'%(ec_out,'es' if ec_out!=1 else ''))

if ec: exit(1)
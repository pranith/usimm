#! /usr/bin/python
import os
import shutil
import sys

SIM_DIR = '/user/dmanatunga/ece8873/assignment3/usimm-v1.3'
RESULTS_DIR = '/user/dmanatunga/ece8873/assignment3/results'
TRACE_DIR = '/user/dmanatunga/ece8873/assignment3/Traces'
TRACE_FILES = {'A': 'Trace_A', 'B': 'Trace_B', 'C': 'Trace_C', 'D': 'Trace_D', 'E': 'Trace_E'}
BENCHMARKS = ['AAAA', 'BBBB', 'CCCC', 'DDDD', 'EEEE', 'ABCD', 'ABCE', 'ABDE', 'ACDE', 'BCDE']
BIN = 'bin/usimm'
SIM = 'usimm'
CONFIG_FILE = 'input/4channel.cfg'
CONFIG = '4channel.cfg'
RUN_FILE = 'run.py'

def run_bench(run_name, bench, base_dir):
    file_name = os.path.join(base_dir, RUN_FILE)
    with open(file_name, 'w') as file:
        file.write('#!/usr/bin/python\n\n')
        file.write('import os\n')
        file.write('import glob\n')
        file.write('import sys\n\n')
        file.write('ppid = os.getppid()\n')
        file.write('test_dir = \'/tmp/memsim_\' + \'%s_\' + str(ppid) + \'/%s\'\n' % (run_name, bench))
        file.write('os.chdir(\'%s\')\n' % (base_dir))
        file.write('os.system(\'uname -a\')\n')
        file.write('os.system(\'mkdir -p \%s\' % (test_dir))\n')
        file.write('os.system(\'mkdir -p \%s/input\' % (test_dir))\n')
        file.write('os.system(\'cp ../%s %%s\' %% (test_dir))\n' % SIM)
        file.write('os.system(\'cp ../%s %%s\' %% (test_dir))\n' % CONFIG)
        file.write('os.system(\'cp ../*.vi %s/input/\' % (test_dir))\n')
        file.write('os.chdir(\'%s\' % (test_dir))\n')
       
        trace_cmd = ''
        results_file = ''
        for trace_name in bench:
            trace_loc = os.path.join(TRACE_DIR, TRACE_FILES[trace_name])
            trace_cmd += trace_loc + ' '
            results_file += 'c' + trace_name + '-'

        results_file += '4_' + run_name


        file.write('os.system(\'./%s %s %s > %s\')\n' % (SIM, CONFIG, trace_cmd, results_file))
        file.write('os.system(\'mv %s %s\')\n' % (results_file, os.path.join(base_dir, results_file)))
        file.write('os.system(\'cp %s %s\')\n' % (os.path.join(base_dir, results_file), os.path.join(SIM_DIR, 'output', results_file)))
       
        file.write('os.system(\'rm -rf %s\' % (test_dir))\n')
        file.close()

    os.system('chmod +x %s' % (file_name))
    
    # qsub command
    cmd = []
    cmd += ['qsub']
    cmd += ['run.py']
    cmd += ['-V -m n']
    cmd += ['-o', '%s/qsub.stdout' % (base_dir)]
    cmd += ['-e', '%s/qsub.stderr' % (base_dir)]
    cmd += ['-q', 'pool1']
    cmd += ['-N', '%s_%s' % (run_name, bench)]
    cmd += ['-l', 'nodes=1:ppn=1']

    cwd = os.getcwd()
    os.chdir('%s' % (base_dir))
    os.system('/bin/echo \'%s\' > %s/RUN_CMD' % (' '.join(cmd), base_dir))
    os.system('%s | tee %s/JOB_ID' % (' '.join(cmd), base_dir))
    os.chdir(cwd)


def memsim(run_name):
    cur_dir = os.getcwd()
    run_dir = os.path.join(RESULTS_DIR, run_name)

    if os.path.exists(run_dir):
        print("Erasing current directory...");
        shutil.rmtree(run_dir)
        
    os.makedirs(run_dir)
    shutil.copyfile(os.path.join(cur_dir, BIN), os.path.join(run_dir, SIM))
    shutil.copystat(os.path.join(cur_dir, BIN), os.path.join(run_dir, SIM))
    os.system('cp %s/input/*.vi %s' % (SIM_DIR, run_dir))
    shutil.copyfile(os.path.join(cur_dir, CONFIG_FILE), os.path.join(run_dir, CONFIG))

    for bench in BENCHMARKS:
        bench_dir = os.path.join(run_dir, bench)
        os.mkdir(bench_dir)
        run_bench(run_name, bench, bench_dir)

if __name__ == "__main__":
    run_name = 'base'
    if len(sys.argv) == 2:
        run_name = sys.argv[1]
    memsim(run_name)

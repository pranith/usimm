#! /usr/bin/python

import os
import sys
import re
import csv

SINGLE_THREAD_TIME = {'A': 424330872,
                      'B': 357830245,
                      'C': 645730097,
                      'D': 362998160,
                      'E': 377036457 }

BENCHMARKS = ['AAAA', 'BBBB', 'CCCC', 'DDDD', 'EEEE', 'ABCD', 'ABCE', 'ABDE', 'ACDE', 'BCDE']
                       

def write_results(data, filename):
    edp_data = [['Workload (EDP)']]
    tot_num_cycle_data = [['Workload (Total Num Cycles)']]
    max_slow_data = [['Workload (Max Slowdown)']]

    for bench in BENCHMARKS:
        edp_data.append([bench])
        tot_num_cycle_data.append([bench])
        max_slow_data.append([bench])
    
    with open(filename, 'w') as results_file:
        results_writer = csv.writer(results_file)
        results_writer.writerow(['Scheduler', 'Total Num Cycles', 'Avg. Max Slowdown', 'PFP', 'Total EDP'])
        for config, config_data in iter(sorted(data.iteritems())):
            with open('%s_results.csv' % config, 'w') as config_file:
                config_writer = csv.writer(config_file)
                config_writer.writerow(['Benchmark', 'Core 0 Cycles', 'Core 1 Cycles', 'Core 2 Cycles', 'Core 3 Cycles',
                                        'Max Slowdown', 'Core 0 Slowdown', 'Core 1 Slowdown', 'Core 2 Slowdown', 'Core 3 Slowdown'])
                i = 0
                edp_data[i].append(config)
                tot_num_cycle_data[i].append(config)
                max_slow_data[i].append(config)
                i = 1
                for bench in BENCHMARKS:
                    row = [bench]
                    bench_data = config_data[bench]
                    for cycle in bench_data['CYCLES']:
                        row.append(cycle)

                    row.append(bench_data['MAX_SLOWDOWN'])
                    for slowdown in bench_data['SLOWDOWN']:
                        row.append(slowdown)

                    config_writer.writerow(row)
                    
                    edp_data[i].append(bench_data['EDP'])
                    tot_num_cycle_data[i].append(bench_data['TOTAL_NUM_CYCLES'])
                    max_slow_data[i].append(bench_data['MAX_SLOWDOWN'])

                    i = i + 1
               
                config_writer.writerow([])
                config_writer.writerow(['TOTAL_NUM_CYCLES', config_data['TOTAL_NUM_CYCLES']])
                config_writer.writerow(['AVG_MAX_SLOWDOWN', config_data['AVG_MAX_SLOWDOWN']])
                config_writer.writerow(['PFP', config_data['PFP']])
                config_writer.writerow([])
                config_writer.writerow(['Benchmark', 'EDP'])
                for bench in BENCHMARKS:
                    row = [bench, config_data[bench]['EDP']]
                    config_writer.writerow(row)
                config_writer.writerow(['Total', config_data['TOTAL_EDP']])

            results_writer.writerow([config, config_data['TOTAL_NUM_CYCLES'],
                                             config_data['AVG_MAX_SLOWDOWN'], 
                                             config_data['PFP'],
                                             config_data['TOTAL_EDP']])

    with open('edp_results.csv', 'w') as edp_file:
        edp_writer = csv.writer(edp_file)
        for row in edp_data:
            edp_writer.writerow(row)

    with open('max_slow_results.csv', 'w') as max_slow_file:
        max_slow_writer = csv.writer(max_slow_file)
        for row in max_slow_data:
            max_slow_writer.writerow(row)

    with open('tot_num_cycle_results.csv', 'w') as tot_num_cycle_file:
        tot_cycle_writer = csv.writer(tot_num_cycle_file)
        for row in tot_num_cycle_data:
            tot_cycle_writer.writerow(row)


def parse_file(filename, core_map):
    data = None
    cycle_regex = re.compile(r'Done: Core (?P<core_id>\d+): Fetched \d+ : Committed \d+ : At time : (?P<cycle>\d+)')
    edp_regex = re.compile(r'Energy Delay product \(EDP\) = (?P<edp>\d*\.?\d+) J\.s')
    
    max_slowdown = 0
    total_num_cycles = 0
    with open(filename, 'r') as data_file:
        data = {'EDP': 0, 'TOTAL_NUM_CYCLES': 0, 'CYCLES': [0]*4, 'MAX_SLOWDOWN': 0, 'SLOWDOWN': [0]*4}
        for line in data_file:
            match = cycle_regex.match(line)
            if match:
                core = match.group('core_id')
                cycles = int(match.group('cycle'))
                slowdown = cycles / float(SINGLE_THREAD_TIME[core_map[core]])
                data['CYCLES'][int(core)] = cycles
                data['SLOWDOWN'][int(core)] = slowdown
                if max_slowdown < slowdown:
                    max_slowdown = slowdown
                total_num_cycles += cycles

            match = edp_regex.match(line)
            if match:
                data['EDP'] = float(match.group('edp'))
            
        data['MAX_SLOWDOWN'] = max_slowdown
        data['TOTAL_NUM_CYCLES'] = total_num_cycles
    return data

def parse_config(directory):
    config_data = {'BENCHMARKS': {}}
    total_num_cycles = 0
    avg_max_slowdown = 0
    total_edp = 0
    for bench in BENCHMARKS:
        core_map = {}
        core_id = 0
        data_file = ''
        for letter in bench:
            core_map[str(core_id)] = letter
            core_id = core_id + 1
            data_file += 'c' + letter + '-'

        data_file += '4_' + directory

        bench_data = parse_file(os.path.join(bench, data_file), core_map)
        config_data[bench] = bench_data
        total_num_cycles += bench_data['TOTAL_NUM_CYCLES']
        avg_max_slowdown += bench_data['MAX_SLOWDOWN']
        total_edp += bench_data['EDP']

    avg_max_slowdown = avg_max_slowdown / len(BENCHMARKS)
    pfp = avg_max_slowdown * total_num_cycles

    config_data['TOTAL_NUM_CYCLES'] = total_num_cycles
    config_data['AVG_MAX_SLOWDOWN'] = avg_max_slowdown
    config_data['PFP'] = pfp
    config_data['TOTAL_EDP'] = total_edp
    return config_data

def parse_results(dirs):
    cur_dir = os.getcwd()
    if len(dirs) == 0:
        config_dirs = [x for x in os.listdir(cur_dir) if os.path.isdir(x)]
        config_dirs.sort()
    else:
        config_dirs = dirs

    results = {}

    for configuration in config_dirs:
        os.chdir(configuration)
        config_data = parse_config(configuration)
        results[configuration] = config_data
        os.chdir(cur_dir)
    
    write_results(results, 'results.csv')

if __name__ == "__main__":
    dirs = []
    if len(sys.argv) > 1:
        dirs = sys.argv[1:]
    parse_results(dirs)

#! /usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, mfgraph, commands
from math import sqrt

#clean up the obj directories
do_clean = False

#build the configs
do_build = False

#run the benchmarks
do_run = True

#collect data to plot
do_collect_data = True

if do_clean and not do_build:
    print "Clean - true and build - false not allowed"
    exit

#set paths
SPECROOT="spec"
print "SPECROOT = " + SPECROOT

configs = []

entry = { "NAME" : "RUN_ALL_BENCHMARKS",
          "NUM_RUNS" : 1,
          "CLEAN_LINE" : " make clean ",
          "BUILD_LINE" : " make ",
          "BUILD_ARCH" : "x86_64",
          "RUN_ARCH" : "x86_64",
          "RUN_LINE_SER" : "./",
          "RUN_LINE" : "./",
          "ARGS" : "",
          }

configs.append(entry);

precs=[
        "PREC_128",
        "PREC_256",
        "PREC_512",
        "PREC_1024",
        ]
filenames = [
        "data_128.txt",
        "data_256.txt",
        "data_512.txt",
        "data_1024.txt",
        ]
ref_cwd = os.getcwd();
fpsanx_home = os.environ['PFSAN_HOME'];
fpsan_home = os.environ['PFSAN_HOME'];
tbb_home = os.environ['TBB_HOME'];
mpfr_home = os.environ['MPFR_HOME'];
arch = platform.machine()
full_hostname = platform.node()
hostname=full_hostname

benchmarks=[
        "amg",
        "milcmk",
]

folder=[
        "AMG",
        "MILCmk",
        ]

inner_folder=[
        "test",
         "",
        ]


executable_orig=[
        "amg",
        "qla_bench-qla-1.7.1-f3",
        ]

executable_pos=[
        "amg_fp",
        "qla_bench-qla-1.7.1-f3_fp",
        ]

inputs_test=[
        " -problem 2 -n 40 40 40",
        "",
        ]

buf_size=[
        "EXPANDED_BUF_SIZE",
        "DEFAULT_BUF_SIZE",
        ]

num_threads=[
        "NUM_T_1",
        "NUM_T_4",
        "NUM_T_8",
        "NUM_T_16",
        "NUM_T_32",
        "NUM_T_64",
        ]

fds = [open(filename, 'w') for filename in filenames]


for j in range(0, len(precs)):
    os.environ["PRECISION"] = precs[j]
    os.environ["CPP"] = "clang++"
    os.environ["CC"] = "clang"
    for config in configs:
        util.log_heading(config["NAME"], character="-")

        for benchmark in benchmarks:
          for i in range(0, config["NUM_RUNS"]):
            util.chdir(ref_cwd)
            rt_str_test = ""
            runtimes_test = []
            runtimes_test_no_tracing = []
            orig_runtimes_test = []

            #with pfsan
            os.environ["SET_CORRECTNESS"] = "PERF"
            os.environ["SET_TRACING"] = "TRACING"
            os.environ["FPSAN_HOME"] = fpsanx_home
            os.environ["BUF_SIZE"] = buf_size[benchmarks.index(benchmark)]
            os.environ["NUM_THREADS"] = "NUM_T_1"
            util.chdir(fpsanx_home+"/"+"runtime")
            util.run_command("make clean", verbose=False)
            util.run_command("make", verbose=False)
            util.chdir(ref_cwd + "/" + SPECROOT + "/" + folder[benchmarks.index(benchmark)])
            util.log_heading(benchmark, character="=")
            if do_run:
                try:
                    clean_string = config["CLEAN_LINE"]
                    util.run_command(clean_string, verbose=False)
                except:
                    print "Clean failed"
            build_string = config["BUILD_LINE"]
            util.run_command(build_string, verbose=False)

            for thread_num in num_threads:
                try:
                    os.environ["NUM_THREADS"] = thread_num
                    util.chdir(fpsanx_home+"/"+"runtime")
                    util.run_command("make clean", verbose=False)
                    util.run_command("make", verbose=False)
                    util.chdir(ref_cwd + "/" + SPECROOT + "/" + folder[benchmarks.index(benchmark)] + "/" + inner_folder[benchmarks.index(benchmark)])
                    run_string = config["RUN_LINE"] + executable_pos[benchmarks.index(benchmark)] + " " + inputs_test[benchmarks.index(benchmark)] 
                    start = time.time()
                    util.run_command(run_string, verbose=False)
                    elapsed = (time.time() - start)
                    runtimes_test_no_tracing.append(elapsed)
                    print "fpsanX:", elapsed

                except util.ExperimentError, e:
                    print "Error: %s" % e
                    print "-----------"
                    print "%s" % e.output
                    continue
            #original runtimes
            run_string = config["RUN_LINE_SER"] + executable_orig[benchmarks.index(benchmark)] + " " + inputs_test[benchmarks.index(benchmark)]
            start = time.time()
            util.run_command(run_string, verbose=False)
            elapsed = (time.time() - start)
            print "baseline:", elapsed

            rt_str_test = benchmark 
            for i in runtimes_test_no_tracing:
              rt_str_test += ":" + str(float(i)) 
            # Add original run times
            rt_str_test += ":" + str(elapsed)
            print rt_str_test
            print ""
            fds[j].write(rt_str_test + "\n")

for fd in fds:
    fd.close()


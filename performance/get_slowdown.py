#! /usr/bin/python
#This script gets the slowdown with 512 bits with and without tracing
import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, mfgraph, commands
from math import sqrt

def geometric_mean(nums):
    return (reduce(lambda x, y: x*y, nums))**(1.0/len(nums))

benchmarks= []
hash_table_results = []

labels = ["Prec-1024", "Prec-512", "Prec-256", "Prec-128"]
i=0
k=0

fd_128 = open("data_128.txt", 'r')
fd_256 = open("data_256.txt", 'r')
fd_512 = open("data_512.txt", 'r')
fd_1024 = open("data_1024.txt", 'r')

slowdown_fpsanx_1 = []
slowdown_fpsanx_128 = []
slowdown_fpsanx_256 = []
slowdown_fpsanx_512 = []
slowdown_fpsanx_1024 = []
i = 0
for line1, line2, line3, line4 in zip(fd_128, fd_256, fd_512, fd_1024):
    (bench_128, core1_128, core4_128, core8_128, core16_128, core32_128, core64_128, base_128)= string.split(line1, ':')
    (bench_256, core1_256, core4_256, core8_256, core16_256, core32_256, core64_256, base_256)= string.split(line2, ':')
    (bench_512, core1_512, core4_512, core8_512, core16_512, core32_512, core64_512, base_512)= string.split(line3, ':')
    (bench_1024, core1_1024, core4_1024, core8_1024, core16_1024, core32_1024, core64_1024, base_1024)= string.split(line4, ':')
    benchmarks.append(bench_128)
    slowdown_128 = float(core64_128)/float(base_128)
    slowdown_256 = float(core64_256)/float(base_256)
    slowdown_512 = float(core64_512)/float(base_512)
    slowdown_1024 = float(core64_1024)/float(base_1024)
    slowdown_fpsanx_1024.append(float(slowdown_1024))
    slowdown_fpsanx_512.append(float(slowdown_512))
    slowdown_fpsanx_256.append(float(slowdown_256))
    slowdown_fpsanx_128.append(float(slowdown_128))
    i += 1
    
benchmarks.append("geomean")
avg_slowdown_128 = geometric_mean(slowdown_fpsanx_128)
print "avg_slowdown_128:"
print avg_slowdown_128
print "\n"
slowdown_fpsanx_128.append(avg_slowdown_128)
avg_slowdown_256 = geometric_mean(slowdown_fpsanx_256)
print "avg_slowdown_256:"
print avg_slowdown_256
print "\n"
slowdown_fpsanx_256.append(avg_slowdown_256)
avg_slowdown_512 = geometric_mean(slowdown_fpsanx_512)
print "avg_slowdown_512:"
print avg_slowdown_512
print "\n"
slowdown_fpsanx_512.append(avg_slowdown_512)
avg_slowdown_1024 = geometric_mean(slowdown_fpsanx_1024)
print "avg_slowdown_1024:"
print avg_slowdown_1024
print "\n"
slowdown_fpsanx_1024.append(avg_slowdown_1024)
hash_table_results.append(slowdown_fpsanx_1024)
hash_table_results.append(slowdown_fpsanx_512)
hash_table_results.append(slowdown_fpsanx_256)
hash_table_results.append(slowdown_fpsanx_128)
k += 4
i += 1

def generate_bar_example():
   stacks=[]
   bars=[]
   output_list = ""
   tempval = 0.5
   tempval1 = 0.0
   for j in range(i):
      bars=[]
      tempval1 = 0
      for kk in range(k):
        numbers = []
        if(float(hash_table_results[kk][j]) > 30):
            if not (int(round(hash_table_results[kk][j])) == 30):
                output_list = output_list + "graph 2 newstring fontsize 5 x " + str(tempval+tempval1) + " y 110 hjc vjt rotate 90.0 : " + str(int(round(hash_table_results[kk][j]))) + "X" + "\n"
            numbers.append(20)
            tempval1 += 0.40
        else:
            numbers.append(hash_table_results[kk][j])
        
        numbers=mfgraph.stack_bars(numbers)
        bars.append([""] + numbers)
        tempval += 0.72

      stacks.append([benchmarks[j]]+ bars)
      tempval += 2.15

   return [mfgraph.stacked_bar_graph(stacks,
       bar_segment_labels = labels,
       title = " ",
       title_fontsize = "20",
       ylabel = "Slowdown",
       colors = ["0.375 0.375 0.375", "0.875 0.875 0.875", "0 0 0", "0.625 0.625 0.625"],
       legend_x = "2",
       legend_y = "80",
       legend_type = "Manual",
       legend_type_x=[1, 10, 20, 30] ,
       legend_type_y=[15, 15, 15, 15],
       #legend_type_x=[1, 12, 20, 30] ,
       #legend_type_y=[10, 10, 10, 10],
       #legend_fontsize = "100",
       legend_fontsize = "9",
       #clip = 300,
       ysize = 1.1,
       xsize = 6,
       ymax = 30,
       patterns = ["solid", "stripe -45", "solid", "stripe 45"],
       stack_name_rotate = 45.0,
       stack_name_font_size = "9",
       label_fontsize = "9",
       yhash_marks = [0, 10, 20, 30],
       yhash_names = ["0X", "10X", "20X", "30X"],
 	) + output_list]
mfgraph.run_jgraph("newpage\n".join(generate_bar_example()), "slowdown")

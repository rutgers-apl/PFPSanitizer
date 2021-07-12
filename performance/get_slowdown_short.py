#! /usr/bin/python
#This script gets the slowdown with 512 bits with and without tracing
import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, mfgraph, commands
from math import sqrt

def geometric_mean(nums):
    return (reduce(lambda x, y: x*y, nums))**(1.0/len(nums))

benchmarks= []
hash_table_results = []

labels = ["Prec-128"]
i=0
k=0

fd_128 = open("data_128.txt", 'r')

slowdown_fpsanx_1 = []
slowdown_fpsanx_128 = []
i = 0
for line1 in fd_128:
    (bench_128, core1_128, core4_128, core8_128, core64_128, base_128)= string.split(line1, ':')
    benchmarks.append(bench_128)
    slowdown_128 = float(core64_128)/float(base_128)
    slowdown_fpsanx_128.append(float(slowdown_128))
    i += 1
    
benchmarks.append("geomean")
avg_slowdown_128 = geometric_mean(slowdown_fpsanx_128)
print "avg_slowdown_128:"
print avg_slowdown_128
print "\n"
slowdown_fpsanx_128.append(avg_slowdown_128)
print "\n"
hash_table_results.append(slowdown_fpsanx_128)
k += 1
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

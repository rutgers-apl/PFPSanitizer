#! /usr/bin/python
#This script gets the speedup with 512 bits with and without tracing
import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, mfgraph, commands
from math import sqrt

def geometric_mean(nums):
    return (reduce(lambda x, y: x*y, nums))**(1.0/len(nums))

benchmarks= []
hash_table_results = []

labels = ["4-Core", "8-Core", "64-Core"]
i=0
k=0

fd_512 = open("data_128.txt", 'r')
speedup_fpsanx_4 = []
speedup_fpsanx_8 = []
speedup_fpsanx_16 = []
speedup_fpsanx_32 = []
speedup_fpsanx_64 = []
i = 0
for line1 in fd_512:
    (bench_512, core1_512, core4_512, core8_512, core64_512, base_512)= string.split(line1, ':')
    benchmarks.append(bench_512)
    speedup_4 = float(core1_512)/float(core4_512)
    speedup_8 = float(core1_512)/float(core8_512)
    speedup_64 = float(core1_512)/float(core64_512)
    speedup_fpsanx_4.append(float(speedup_4))
    speedup_fpsanx_8.append(float(speedup_8))
    speedup_fpsanx_64.append(float(speedup_64))
    i += 1
    
benchmarks.append("geomean")
avg_speedup_4 = geometric_mean(speedup_fpsanx_4)
print "avg_speedup_4:", avg_speedup_4
print "\n"
speedup_fpsanx_4.append(avg_speedup_4)

avg_speedup_8 = geometric_mean(speedup_fpsanx_8)
print "avg_speedup_8:",avg_speedup_8
print "\n"
speedup_fpsanx_8.append(avg_speedup_8)

avg_speedup_64 = geometric_mean(speedup_fpsanx_64)
print "avg_speedup_64:",avg_speedup_64
print "\n"
speedup_fpsanx_64.append(avg_speedup_64)

hash_table_results.append(speedup_fpsanx_4)
hash_table_results.append(speedup_fpsanx_8)
hash_table_results.append(speedup_fpsanx_64)
k += 3
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
        if(float(hash_table_results[kk][j]) > 60):
            if not (int(round(hash_table_results[kk][j])) == 60):
                output_list = output_list + "graph 2 newstring fontsize 5 x " + str(tempval+tempval1) + " y 110 hjc vjt rotate 90.0 : " + str(int(round(hash_table_results[kk][j]))) + "X" + "\n"
            numbers.append(60)
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
       ylabel = "Speedup",
       colors = ["0.375 0.375 0.375", "0.875 0.875 0.875", "0 0 0", "0.625 0.625 0.625"],
       legend_x = "2",
       legend_y = "80",
       legend_type = "Manual",
       legend_type_x=[1, 10, 20, 30, 40] ,
       legend_type_y=[15, 15, 15, 15, 15],
       #legend_type_x=[1, 12, 20, 30] ,
       #legend_type_y=[10, 10, 10, 10],
       #legend_fontsize = "100",
       legend_fontsize = "9",
       #clip = 300,
       ysize = 1.1,
       xsize = 6,
       ymax = 60,
       patterns = ["solid", "stripe -45", "solid", "stripe 45"],
       stack_name_rotate = 45.0,
       stack_name_font_size = "9",
       label_fontsize = "9",
       yhash_marks = [0, 20, 40, 60],
       yhash_names = ["0X", "20X", "40X", "60X"],
 	) + output_list]
mfgraph.run_jgraph("newpage\n".join(generate_bar_example()), "speedup")

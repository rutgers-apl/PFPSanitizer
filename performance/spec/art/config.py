{
    "NAME" : "spec2k-art",
    "CFILES" : "scanner.c",
    "CFLAGS" : " -I../../../ -DSPEC_CPU_LP64   -lm ",
    "CCURED_FLAGS" : "",
    "LLVM_LD_FLAGS" : " -internalize -ipsccp -globalopt -constmerge -deadargelim -instcombine -basiccg -inline -prune-eh -globalopt -globaldce -basiccg -argpromotion -instcombine -jump-threading -domtree -domfrontier -scalarrepl -basiccg -globalsmodref-aa -domtree -loops -loopsimplify -domfrontier -scalar-evolution  -licm -memdep -gvn -memdep -memcpyopt -dse -instcombine -jump-threading -domtree -domfrontier -mem2reg -simplifycfg -globaldce -instcombine -instcombine -simplifycfg -adce -globaldce -preverify -domtree -verify ",
#    "FAST_INPUT" : "lbm.in",
#    "SLOW_INPUT" : "lbm.in",
    "COPY_INPUT" : " ../../../hc.img ../../../c756hel.in ../../../a10.img ",
    "FAST_COMMANDLINE" : "-scanfile ../../../c756hel.in -trainfile1 ../../../a10.img -trainfile2 ../../../hc.img -stride 2 -startx 110 -starty 200 -endx 160 -endy 240 -objects 10",
    "SLOW_COMMANDLINE" : "-scanfile ../../../c756hel.in -trainfile1 ../../../a10.img -trainfile2 ../../../hc.img -stride 2 -startx 110 -starty 200 -endx 160 -endy 240 -objects 10",
    "SIM_COMMANDLINE" : "-scanfile ../../../c756hel.in -trainfile1 ../../../a10.img -trainfile2 ../../../hc.img -stride 2 -startx 110 -starty 200 -endx 160 -endy 240 -objects 8",
#    "SLOW_COMMANDLINE" : "../../ref-inp.in",


    "SIM_EXP_COMMAND_LINE" : "-scanfile ../../../c756hel.in -trainfile1 ../../../a10.img -trainfile2 ../../../hc.img -stride 2 -startx 110 -starty 200 -endx 160 -endy 240 -objects 8",
    "SIM_COPY_INPUT": ["hc.img", "c756hel.in", "a10.img"],
    "SIM_NEW_COMMANDLINE" : "-scanfile inputs/c756hel.in -trainfile1 inputs/a10.img -trainfile2  inputs/hc.img -stride 2 -startx 110 -starty 200 -endx 160 -endy 240 -objects 8",

    }
    

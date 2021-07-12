{
    "NAME" : "spec2k-ammp",
    "CFILES" : "ammp.c analyze.c angle.c animate.c anonbon.c atoms.c bonds.c box.c eval.c gsdg.c hybrid.c math.c mom.c monitor.c noel.c optimist.c random.c rectmm.c restrain.c significance.c tailor.c tether.c tgroup.c torsion.c tset.c unonbon.c variable.c vnonbon.c ",
    "CFLAGS" : " -I../../../ -DSPEC_CPU_LP64 -DSPEC_CPU2000   -lm ",
    "CCURED_FLAGS" : "",
    "LLVM_LD_FLAGS" : " -internalize -ipsccp -globalopt -constmerge -deadargelim -instcombine -basiccg -inline -prune-eh -globalopt -globaldce -basiccg -argpromotion -instcombine -jump-threading -domtree -domfrontier -scalarrepl -basiccg -globalsmodref-aa -domtree -loops -loopsimplify -domfrontier -scalar-evolution  -licm -memdep -gvn -memdep -memcpyopt -dse -instcombine -jump-threading -domtree -domfrontier -mem2reg -simplifycfg -globaldce -instcombine -instcombine -simplifycfg -adce -globaldce -preverify -domtree -verify ",
    "FAST_INPUT" : "ammp.in",
    "SLOW_INPUT" : "ammp.in",
    "SIM_INPUT" : "sim_ammp.in",
    "FAST_COMMANDLINE" : "",
    "SLOW_COMMANDLINE" : "",
#    "SLOW_COMMANDLINE" : "../../ref-inp.in",

    "SIM_EXP_COMMANDLINE" : "",
    "SIM_EXP_INPUT" : " -functional:syscall:stdin ../../../sim_ammp.in ",

    "TRAIN_INPUT" : "../data/train/input/ammp.in",
    "TEST_INPUT" : "../data/test/input/ammp.in",
    
    "SIM_COPY_INPUT" : ["new_ammp.in", "all.new.ammp", "new.tether"],

    "SIM_NEW_INPUT" : "-functional:syscall:stdin inputs/new_ammp.in",

    }
    

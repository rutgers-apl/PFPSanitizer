TARGET   = libfpsanitizer
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
TD_ROOT := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))
CXX=g++
LINKER=g++
LDFLAGS+= -shared
SET_OPT := $(if $(SET_DEBUG),-O0, -O3)
OPT=$(SET_OPT)


COUNT_THREADS := $(if $(NUM_THREADS),$(NUM_THREADS), "DEFAULT_NUM_THREADS")
#BUFFER_SIZE := $(if $(BUF_SIZE),$(BUF_SIZE), "DEFAULT_BUF_SIZE")
BUFFER_SIZE := $(if $(BUF_SIZE),$(BUF_SIZE), "EXPANDED_BUF_SIZE")
PREC := $(if $(PRECISION),$(PRECISION), "PREC_512")
SELECTIVE := $(if $(SET_SELECTIVE), $(SET_SELECTIVE), "SELECTIVE")
CORRECTNESS := $(if $(SET_CORRECTNESS), $(SET_CORRECTNESS), "CORRECTNESS")
#To enable tracing run - export SET_TRACING=TRACING
TRACING := $(if $(SET_TRACING), $(SET_TRACING), "TRACING")
CXXFLAGS+= -D $(CORRECTNESS) -D $(SELECTIVE) -D $(COUNT_THREADS) -D $(BUFFER_SIZE) -D $(TRACING) -D $(PREC) -fPIC -g -std=c++11 -rdynamic -Wall -lmpfr -pthread -ltbb 

#INCPATH=-I. -I$(TBB_HOME)/include -I$(MPFR_HOME)/src/ 
INCPATH=-I. -I$(MPFR_HOME)/src/ 
ARFLAGS=-rcs

# change these to set the proper directories where each files shoould be
SRCDIR = .
INCDIR = .
OBJDIR = obj
BINDIR = bin

SOURCES  := $(wildcard $(SRCDIR)/*.cpp)
INCLUDES := $(wildcard $(INCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
rm       = rm -rf

all:
	@test -d $(OBJDIR) || mkdir $(OBJDIR)
	@make tool

tool: $(OBJECTS)
	@$(LINKER) $(OBJECTS) $(LDFLAGS) -o $(TARGET).so
	@mv $(TARGET).so $(OBJDIR)
	@echo "Linking complete!"

$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CXX) $(OPT) $(CXXFLAGS) -c $< $(INCPATH) -o $@
	@echo "Compiled "$<" successfully!"

.PHONY: clean

clean:
	@$(rm) $(OBJDIR)
	@$(rm) *~
	@echo "Cleanup complete!"

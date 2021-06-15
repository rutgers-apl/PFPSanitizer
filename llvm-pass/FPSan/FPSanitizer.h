//===-FPSanitizer.h  - Interface ---------------------------------*- C++
//-*-===//
//
//
//
//===----------------------------------------------------------------------===//
//
// This pass instruments floating point instructions by inserting
// calls to the runtime to perform shadow execution with arbitrary
// precision.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <fstream>
#include <queue>
#include <set>

using namespace llvm;

namespace {
struct FPSanitizer : public ModulePass {

public:
  FPSanitizer() : ModulePass(ID) {}

  virtual bool runOnModule(Module &module);
  Function *removeArgs(Function *F, Function *OldF, Value *BufAddr);
  void createPhiCounter(BasicBlock *BB);
  void addIncomingPhi(BasicBlock *BB);
  void propagatePhiCounter(BasicBlock *BB);
  void propagatePhiCounter1(BasicBlock *BB);
  void updateUpdateBlock(BasicBlock *BB);
  void handleStoreIdxProducer(Instruction *I, BasicBlock *BB, Function *F);
  void handleStoreIdxProducerCallInst(Instruction *I, BasicBlock *BB, Function *F);
  void handleSliceFlag(Function *F);
  void updateBuffer(Value *CI, Instruction *Next, Function *OldF);
  void updateBufferNewBlock(Module *M);
  void updateBufferNewBlockArg(Module *M);
  void handleIdxLoad(Function *F);
  Value* createUpdateBlockArg(Value *Addr, 
                              Instruction *I,  
                              Function *F, Value*);
  void createUpdateBlock(Value *val, Value *Addr, 
                              Instruction *I, BasicBlock *OldB, 
                              BasicBlock *ContB, Function *F);
  void createUpdateBlock2(Value *VAddr, Value *Val, Value *Addr, 
                          Instruction *I, BasicBlock *OldB, 
                          BasicBlock *ContB, Function *F);
  void createUpdateBlock3(Value *VAddr1, Value *VAddr2, Value *Val, Value *Addr, 
                          Instruction *I, BasicBlock *OldB, 
                          BasicBlock *ContB, Function *F);
  Value* readFromBuf(Instruction *I, BasicBlock *BB, Function *F);
  Function *cloneFunction(Function *F);
  Function *addIndexProducer(Function *F);
  void SetBufIndexShadow(Function *F);
  void SetBufIndex(Function *F, Function *SF);
  void CloneFunction(Function *NewFunc, const Function *OldFunc,
                     ValueToValueMapTy &VMap, bool ModuleLevelChanges,
                     SmallVectorImpl<ReturnInst *> &Returns,
                     const char *NameSuffix, ClonedCodeInfo *CodeInfo = nullptr,
                     ValueMapTypeRemapper *TypeMapper = nullptr,
                     ValueMaterializer *Materializer = nullptr);
  void CloneFunctionArgs(Function *NewFunc, const Function *OldFunc,
                         ValueToValueMapTy &VMap, bool ModuleLevelChanges,
                         SmallVectorImpl<ReturnInst *> &Returns,
                         const char *NameSuffix,
                         ClonedCodeInfo *CodeInfo = nullptr,
                         ValueMapTypeRemapper *TypeMapper = nullptr,
                         ValueMaterializer *Materializer = nullptr);
  void removeOldInst();
  bool isFloatInst(Instruction *I);
  void RemoveEveryThingButFloat(Function *F);
  void handleBranch(Function *F);
  void createInitMpfr(Value *BOGEP, Function *F, AllocaInst *Alloca,
                      size_t index);
  void createInitAndSetMpfr(Value *BOGEP, Function *F, AllocaInst *Alloca,
                            size_t index, Value *OP);
  void createInitAndSetP32(Value *BOGEP, Function *F, AllocaInst *Alloca,
                           size_t index, Value *OP);
  void instrumentAllFunctions(std::string FN);
  void createMpfrAlloca(Function *F);
  void callGetArgument(Function *F);
  AllocaInst *createAlloca(Function *F, size_t InsCount);
  void createGEP(Function *F, AllocaInst *Alloca, long TotalAlloca);
  void clearAlloca(Function *F, size_t InsCount);
  Instruction *getNextInstruction(Instruction *I, BasicBlock *BB);
  Instruction *getNextInstructionNotPhi(Instruction *I, BasicBlock *BB);
  void findInterestingFunctions(Function *F);
  bool handleOperand(Instruction *I, Value *OP, Function *F, Value **InstIdx);
  void handleSToFProducer(SIToFPInst *SI, BasicBlock *BB, Function *F);
  void handleUToFProducer(UIToFPInst *SI, BasicBlock *BB, Function *F);
  void handleBToFProducer(BitCastInst *SI, BasicBlock *BB, Function *F);
  void handleSToF(SIToFPInst *SI, BasicBlock *BB, Function *F);
  void handleUToF(UIToFPInst *SI, BasicBlock *BB, Function *F);
  void handleBToF(BitCastInst *SI, BasicBlock *BB, Function *F);
  void handleStore(StoreInst *SI, BasicBlock *BB, Function *F);
  void handleStoreProducer(StoreInst *SI, BasicBlock *BB, Function *F);
  void handleMemCpy(CallInst *CI, BasicBlock *BB, Function *F,
                    std::string CallName);
  void handleMemCpyProducer(CallInst *CI, BasicBlock *BB, Function *F,
                            std::string CallName);
  void handleMemset(CallInst *CI, BasicBlock *BB, Function *F,
                    std::string CallName);
  void handleMemsetProducer(CallInst *CI, BasicBlock *BB, Function *F,
                            std::string CallName);
  void handleNewPhi(Function *F);
  void copyPhi(Instruction *I, Function *F);
  void handlePhi(PHINode *PN, BasicBlock *BB, Function *F);
  void handleSelect(SelectInst *SI, BasicBlock *BB, Function *F);
  void handleSelectProducer(SelectInst *SI, BasicBlock *BB, Function *F);
  void handleBinOp(BinaryOperator *BO, BasicBlock *BB, Function *F);
  void handleFNeg(UnaryOperator *UO, BasicBlock *BB, Function *F);
  void handleFcmp(FCmpInst *FCI, BasicBlock *BB, Function *F);
  void handleFcmpProducer(FCmpInst *FCI, BasicBlock *BB, Function *F);
  void handleReturn(ReturnInst *RI, BasicBlock *BB, Function *F);
  bool checkIfBitcastFromFP(BitCastInst *BI);
  int getBitcastFromFPType(BitCastInst *BI);
  void handleLoadProducer(LoadInst *LI, BasicBlock *BB, Function *F);
  void handleLoad(LoadInst *LI, BasicBlock *BB, Function *F);
  void handleAlloca(AllocaInst *AI, BasicBlock *BB, Function *F);
  void handleMathLibFunc(CallInst *CI, BasicBlock *BB, Function *F,
                         std::string Name);
  void handleMathLibFuncProducer(CallInst *CI, BasicBlock *BB, Function *F);
  void handleBinOpProducer(BinaryOperator *BO, BasicBlock *BB, Function *F);
  void handlePositLibFunc(CallInst *CI, BasicBlock *BB, Function *F,
                          std::string Name);
  void handleCallInstIndirect(CallInst *CI, BasicBlock *BB, Function *F);
  void handleCallInst(CallInst *CI, BasicBlock *BB, Function *F);
  void handleEndSliceCallInstP(CallInst *CI, Function *F);
  void handleStartSliceCallInstP(CallInst *CI, Function *F);
  void getBufAddress(CallInst *CI, Function *F);
  void handleICallInstProducer(CallInst *CI, BasicBlock *BB, Function *F);
  void handleICallInst(CallInst *CI, Function *F);
  void handleCallInstProducer(CallInst *CI, BasicBlock *BB, Function *F);
  void handleCallInstProducerMain(CallInst *CI, Function *F);
  void handlePrint(CallInst *CI, BasicBlock *BB, Function *F);
  bool isListedFunction(StringRef FN, std::string FileName);
  void addFunctionsToList(std::string FN);
  bool isFloatType(Type *InsType);
  bool isFloat(Type *InsType);
  bool isDouble(Type *InsType);
  bool isArrayFloat(ArrayType *AT);
  bool isPointerFloat(PointerType *AT);
  bool getArrayFloatType(ArrayType *AT);
  bool getPointerFloatType(PointerType *AT);
  void handleMainRet(Function *F);
  void handleStartSliceC(Function *F);
  void handleEndSliceC(Function *F);
  void handleStartSliceP(Function *F);
  void handleEndSliceP(Function *F);
  void handleFuncInit(Function *F);
  void handleFuncMainInit(Function *F);
  void handleFuncShadowMainInit(Function *F);
  void handleInit(Module *M);
  void handleIns(Instruction *I, BasicBlock *BB, Function *F);
  long getTotalFPInst(Function *F);
  bool isFunctionPointerType(Type *Type);
  Type* getFunctionPointerType(Type *Type);
  ConstantInt *GetInstId(Function *F, Instruction *I);
  StructType *MPFRTy;
  Type *MPtrTy;
  Type *RealPtr;
  StructType *Real;
  int index = 0;
  Value *Queue;
  GlobalVariable *QIdx;
  GlobalVariable *CQIdx;
  GlobalVariable *SliceFlag;
  std::map<BasicBlock *, Instruction*> QIdxMap;
  std::map<Function *, Instruction*> CQIdxMap;
  std::map<Function *, Instruction*> SliceFlagMap;
  std::map<Instruction *, Instruction *> FCMPMapPush;
  std::map<Instruction *, Instruction *> FCMPMap;
  std::map<Function *, Value *> BufAddrMap;
  std::map<Function *, Value *> CBufAddrMap;
  std::map<Function *, Value *> BufIndexMap;
  std::map<CallInst *, Instruction *> CallIdxMap;
  std::map<Function *, Value *> BufIdxMap;
  std::map<Function *, Instruction *> ReturnMap;
  std::map<Value *, Value *> ConsMap;
  std::map<Function *, Function *> CloneFuncMap;
  std::map<Function *, Function *> OrigFuncMap;
  std::map<Instruction *, Value *> GEPMap;
  std::map<ConstantFP *, Value *> ConstantMap;
  // map new instruction with old esp. for select and phi
  std::map<Instruction *, Instruction *> RegIdMap;
  std::map<Instruction *, Type *> InstTypeMap;
  std::map<Instruction *, Instruction *> NewPhiMap;
  // track unique index for instructions
  std::map<Instruction *, Instruction *> MInsMap;
  std::map<Instruction *, Instruction *> CondMap;
  std::map<Argument *, Instruction *> MArgMap;
  // Arguments can not be instruction, so we need a seperate map to hold indexes
  // for arguments
  std::map<Argument *, size_t> ArgMap;
  std::map<Function *, size_t> FuncTotalArg;
  std::set<StringRef> LibFuncList;
  SmallVector<Instruction *, 8> InstList;
  SmallVector<CallInst *, 8> CallList;
  // list of all functions need to be instrumented
  SmallVector<Value *, 8> StoreList;
  SmallVector<Value *, 8> GEPList;
  SmallVector<Function *, 8> SliceList;
  SmallVector<Value *, 8> AllCreatedList;
  SmallVector<CallInst **, 8> MemList;
  SmallVector<Function *, 8> AllFuncList;
  SmallVector<Function *, 8> AllOrigFuncList;
  SmallVector<Instruction *, 8> AllInstList;
  SmallVector<Instruction *, 8> AllBranchList;
  std::vector<Value *> ValList;
  SmallVector<Type *, 4> ValType;
  static char ID; // Pass identification
  long InsCount = 0;
  std::function<const TargetLibraryInfo &(Function &F)> GetTLI;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
  }

private:
  FunctionCallee Func;
  FunctionCallee LoadCall;
  FunctionCallee ComputeReal;
  FunctionCallee FuncExit;
  FunctionCallee CheckBranch;
  FunctionCallee FuncInit;
  FunctionCallee UpdateBufferFunc;
  FunctionCallee UpdateBufferFuncArg;
  FunctionCallee CheckErr;
  FunctionCallee Finish;
  FunctionCallee HandleFunc;
  FunctionCallee SetRealTemp;
  FunctionCallee AddFunArg;
  Value *Counter;
  PHINode *iPHI = nullptr;
};
} // namespace

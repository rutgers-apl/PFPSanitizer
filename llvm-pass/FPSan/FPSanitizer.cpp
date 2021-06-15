#include "FPSanitizer.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/ConstantFolder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/CFG.h"
#include <set>

#define DEBUG_P 0

void FPSanitizer::addFunctionsToList(std::string FN) {
  std::ofstream myfile;
  myfile.open("functions.txt", std::ios::out | std::ios::app);
  if (isListedFunction(FN, "forbid.txt"))
    return;
  if (myfile.is_open()) {
    myfile << FN;
    myfile << "\n";
    myfile.close();
  }
}

// check name of the function and check if it is in list of functions given by
// developer and return true else false.
bool FPSanitizer::isListedFunction(StringRef FN, std::string FileName) {
  std::ifstream infile(FileName);
  std::string line;
  while (std::getline(infile, line)) {
    if (FN.compare(line) == 0) {
      return true;
    }
  }
  return false;
}

Type* FPSanitizer::getFunctionPointerType(Type *Type){
  if(PointerType *PtrType=dyn_cast<PointerType>(Type)){
    return getFunctionPointerType(PtrType->getElementType());
  }
  else if(Type->isFunctionTy()){
    return  Type;
  }
  return nullptr;
}

bool FPSanitizer::isFunctionPointerType(Type *Type){
  if(PointerType *PtrType=dyn_cast<PointerType>(Type)){
    return isFunctionPointerType(PtrType->getElementType());
  }
  else if(Type->isFunctionTy()){
    return  true;
  }
  return false;
}

bool FPSanitizer::isFloatType(Type *InsType) {
  if (InsType->getTypeID() == Type::DoubleTyID ||
      InsType->getTypeID() == Type::FloatTyID)
    return true;
  return false;
}

bool FPSanitizer::isFloat(Type *InsType) {
  if (InsType->getTypeID() == Type::FloatTyID)
    return true;
  return false;
}
bool FPSanitizer::isDouble(Type *InsType) {
  if (InsType->getTypeID() == Type::DoubleTyID)
    return true;
  return false;
}

ConstantInt *FPSanitizer::GetInstId(Function *F, Instruction *I) {
  Module *M = F->getParent();
  MDNode *uniqueIdMDNode = I->getMetadata("fpsan_inst_id");
  if (uniqueIdMDNode == NULL) {
    return ConstantInt::get(Type::getInt64Ty(M->getContext()), 0);
  }

  Metadata *uniqueIdMetadata = uniqueIdMDNode->getOperand(0).get();
  ConstantAsMetadata *uniqueIdMD =
      dyn_cast<ConstantAsMetadata>(uniqueIdMetadata);
  Constant *uniqueIdConstant = uniqueIdMD->getValue();
  return dyn_cast<ConstantInt>(uniqueIdConstant);
}

void FPSanitizer::createGEP(Function *F, AllocaInst *Alloca, long TotalAlloca) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  Instruction *I = dyn_cast<Instruction>(Alloca);
  Module *M = F->getParent();
  Instruction *Next = getNextInstruction(I, &BB);

  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);

  IRBuilder<> IRB(Next);
  Instruction *End;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (dyn_cast<ReturnInst>(&I)) {
        End = &I;
      }
    }
  }
  IRBuilder<> IRBE(End);
  int index = 0;

  Type *VoidTy = Type::getVoidTy(M->getContext());

  for (auto &BB : *F) {
    Value *Idx = BufIdxMap.at(F);
    for (auto &I : BB) {
      if (BinaryOperator *BO = dyn_cast<BinaryOperator>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        switch (BO->getOpcode()) {
        case Instruction::FAdd:
        case Instruction::FSub:
        case Instruction::FMul:
        case Instruction::FDiv: {
          if (index - 1 > TotalAlloca) {
            errs() << "Error\n\n\n: index > TotalAlloca " << index << ":"
                   << TotalAlloca << "\n";
          }
          Value *Indices[] = {
              ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
              ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
          Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
          GEPMap.insert(std::pair<Instruction *, Value *>(&I, BOGEP));
          Value *Op1 = BO->getOperand(0);
          Value *Op2 = BO->getOperand(1);

          FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty,
                                            MPtrTy);
          IRB.CreateCall(FuncInit, {Idx, BOGEP});

          FuncInit =
              M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
          IRBE.CreateCall(FuncInit, {BOGEP});

          index++;

          if (isa<ConstantFP>(Op1)) {
            if (index - 1 > TotalAlloca) {
              errs() << "Error\n\n\n: index > TotalAlloca " << index << ":"
                     << TotalAlloca << "\n";
            }
            Value *Indices[] = {
                ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
                ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
            Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
            GEPMap.insert(std::pair<Instruction *, Value *>(
                dyn_cast<Instruction>(Op1), BOGEP));

            FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
                                              Int32Ty, MPtrTy);
            IRB.CreateCall(FuncInit, {Idx, BOGEP});

            if (isFloat(Op1->getType())) {
              FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst_val",
                                                VoidTy, Int32Ty, MPtrTy,
                                                Op1->getType(), instId->getType(), Int32Ty);

            } else if (isDouble(Op1->getType())) {
              FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_dconst_val",
                                                VoidTy, Int32Ty, MPtrTy,
                                                Op1->getType(), instId->getType(), Int32Ty);
            }
            IRB.CreateCall(FuncInit, {Idx, BOGEP, Op1, instId, lineNumber});
            ConsMap.insert(std::pair<Value *, Value *>(Op1, BOGEP));

            FuncInit =
                M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
            IRBE.CreateCall(FuncInit, {BOGEP});

            index++;
          }
          if (isa<ConstantFP>(Op2)) {
            if (index - 1 > TotalAlloca) {
              errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                     << TotalAlloca << "\n";
            }
            Value *Indices[] = {
                ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
                ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
            Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
            GEPMap.insert(std::pair<Instruction *, Value *>(
                dyn_cast<Instruction>(Op2), BOGEP));

            FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
                                              Int32Ty, MPtrTy);
            IRB.CreateCall(FuncInit, {Idx, BOGEP});

            if (isFloat(Op2->getType())) {

              FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst_val",
                                                VoidTy, Int32Ty, MPtrTy,
                                                Op2->getType(), instId->getType(), Int32Ty);

            } else if (isDouble(Op2->getType())) {

              FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_dconst_val",
                                                VoidTy, Int32Ty, MPtrTy,
                                                Op2->getType(), instId->getType(), Int32Ty);
            }
            IRB.CreateCall(FuncInit, {Idx, BOGEP, Op2, instId, lineNumber});
            ConsMap.insert(std::pair<Value *, Value *>(Op2, BOGEP));

            FuncInit =
                M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
            IRBE.CreateCall(FuncInit, {BOGEP});
            index++;
          }
        }
        }
      } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        switch (UO->getOpcode()) {
        case Instruction::FNeg: {
          Instruction *Next = getNextInstruction(UO, &BB);
          IRBuilder<> IRBI(Next);
          if (index - 1 > TotalAlloca) {
            errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                   << TotalAlloca << "\n";
          }
          Value *Indices[] = {
              ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
              ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
          Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
          GEPMap.insert(std::pair<Instruction *, Value *>(
              dyn_cast<Instruction>(UO), BOGEP));

          FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty,
                                            MPtrTy);
          IRB.CreateCall(FuncInit, {Idx, BOGEP});

          FuncInit =
              M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
          IRBE.CreateCall(FuncInit, {BOGEP});
          index++;
        }
        }
      } else if (SIToFPInst *UI = dyn_cast<SIToFPInst>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        Instruction *Next = getNextInstruction(UI, &BB);
        IRBuilder<> IRBI(Next);
        if (index - 1 > TotalAlloca) {
          errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                 << TotalAlloca << "\n";
        }
        Value *Indices[] = {
            ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
            ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
        Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
        GEPMap.insert(std::pair<Instruction *, Value *>(
            dyn_cast<Instruction>(UI), BOGEP));

        FuncInit =
            M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty, MPtrTy);
        IRB.CreateCall(FuncInit, {Idx, BOGEP});
        if (isFloat(UI->getType())) {
          FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst", VoidTy,
                                     Int32Ty, Type::getFloatTy(M->getContext()), 
                                     MPtrTy, instId->getType(), Int32Ty);
        } else if (isDouble(UI->getType())) {
          FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_dconst", VoidTy, 
                                     Int32Ty, Type::getDoubleTy(M->getContext()), 
                                     MPtrTy, instId->getType(), Int32Ty);
        }
        Value *Val = readFromBuf(&I, &BB, F);
        if (isFloat(UI->getType())) {
          Val = IRBI.CreateFPTrunc(Val, Type::getFloatTy(M->getContext()));
        }

        Value *CS = IRBI.CreateCall(FuncInit, {Idx, Val, BOGEP, instId, lineNumber});
        ConsMap.insert(std::pair<Value *, Value *>(UI, BOGEP));

        FuncInit = M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
        IRBE.CreateCall(FuncInit, {BOGEP});
        index++;
      } else if (BitCastInst *UI = dyn_cast<BitCastInst>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        if (isFloatType(UI->getType())) {
          Instruction *Next = getNextInstruction(UI, &BB);
          IRBuilder<> IRBI(Next);
          if (index - 1 > TotalAlloca) {
            errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                   << TotalAlloca << "\n";
          }
          Value *Indices[] = {
              ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
              ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
          Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
          GEPMap.insert(std::pair<Instruction *, Value *>(
              dyn_cast<Instruction>(UI), BOGEP));

          FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty, MPtrTy);
          IRB.CreateCall(FuncInit, {Idx, BOGEP});
          if (isFloat(UI->getType())) {
            FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst", VoidTy,
                                       Int32Ty, Type::getFloatTy(M->getContext()),
                                       MPtrTy, instId->getType(), Int32Ty);
          } else if (isDouble(UI->getType())) {
            FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_dconst", VoidTy,
                                       Int32Ty, Type::getDoubleTy(M->getContext()),
                                       MPtrTy, instId->getType(), Int32Ty);
          }

          Value *Val = readFromBuf(&I, &BB, F);
          if (isFloat(UI->getType())) {
            Val = IRBI.CreateFPTrunc(Val, Type::getFloatTy(M->getContext()));
          }
          IRBI.CreateCall(FuncInit, {Idx, Val, BOGEP, instId, lineNumber});
          ConsMap.insert(std::pair<Value *, Value *>(UI, BOGEP));
          FuncInit =
              M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
          IRBE.CreateCall(FuncInit, {BOGEP});

          index++;
        }
      } else if (UIToFPInst *UI = dyn_cast<UIToFPInst>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        Instruction *Next = getNextInstruction(UI, &BB);
        IRBuilder<> IRBI(Next);
        if (index - 1 > TotalAlloca) {
          errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                 << TotalAlloca << "\n";
        }
        Value *Indices[] = {
            ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
            ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
        Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
        GEPMap.insert(std::pair<Instruction *, Value *>(
            dyn_cast<Instruction>(UI), BOGEP));

        FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty, MPtrTy);
        IRB.CreateCall(FuncInit, {Idx, BOGEP});
        if (isFloat(UI->getType())) {
          FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst", VoidTy,
                                     Int32Ty, Type::getFloatTy(M->getContext()), 
                                     MPtrTy, instId->getType(), Int32Ty);
        } else if (isDouble(UI->getType())) {
          FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_dconst", VoidTy,
                                     Int32Ty, Type::getDoubleTy(M->getContext()), 
                                     MPtrTy, instId->getType(), Int32Ty);
        }
        Value *Val = readFromBuf(&I, &BB, F);
        if (isFloat(UI->getType())) {
          Val = IRBI.CreateFPTrunc(Val, Type::getFloatTy(M->getContext()));
        }
        IRBI.CreateCall(FuncInit, {Idx, Val, BOGEP, instId, lineNumber});
        ConsMap.insert(std::pair<Value *, Value *>(UI, BOGEP));

        FuncInit = M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
        IRBE.CreateCall(FuncInit, {BOGEP});

        index++;
      } else if (FCmpInst *FCI = dyn_cast<FCmpInst>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        Value *Op1 = FCI->getOperand(0);
        Value *Op2 = FCI->getOperand(1);

        bool Op1Call = false;
        bool Op2Call = false;

        if (isa<ConstantFP>(Op1) || Op1Call) {
          if (index - 1 > TotalAlloca) {
            errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                   << TotalAlloca << "\n";
          }
          Value *Indices[] = {
              ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
              ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
          Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
          GEPMap.insert(std::pair<Instruction *, Value *>(
              dyn_cast<Instruction>(Op1), BOGEP));

          FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty,
                                            MPtrTy);
          IRB.CreateCall(FuncInit, {Idx, BOGEP});

          if (isFloat(Op1->getType())) {
            FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst_val",
                                              VoidTy, Int32Ty, MPtrTy,
                                              Op1->getType(), instId->getType(), Int32Ty);
          } else if (isDouble(Op1->getType())) {

            FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_dconst_val",
                                              VoidTy, Int32Ty, MPtrTy,
                                              Op1->getType(), instId->getType(), Int32Ty);
          }
          if (Op1Call) {
            Instruction *Next = getNextInstruction(FCI, &BB);
            IRBuilder<> IRBI(Next);
            IRBI.CreateCall(FuncInit, {Idx, BOGEP, Op1, instId, lineNumber});
          } else
            IRB.CreateCall(FuncInit, {Idx, BOGEP, Op1, instId, lineNumber});
          ConsMap.insert(std::pair<Value *, Value *>(Op1, BOGEP));

          FuncInit =
              M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
          IRBE.CreateCall(FuncInit, {BOGEP});
          index++;
        }
        if (isa<ConstantFP>(Op2) || Op2Call) {
          if (index - 1 > TotalAlloca) {
            errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                   << TotalAlloca << "\n";
          }
          Value *Indices[] = {
              ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
              ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
          Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
          GEPMap.insert(std::pair<Instruction *, Value *>(
              dyn_cast<Instruction>(Op2), BOGEP));

          FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty,
                                            MPtrTy);
          IRB.CreateCall(FuncInit, {Idx, BOGEP});

          if (isFloat(Op2->getType())) {
            FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst_val",
                                              VoidTy, Int32Ty, MPtrTy,
                                              Op2->getType(), instId->getType(), Int32Ty);
          } else if (isDouble(Op2->getType())) {

            FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_dconst_val",
                                              VoidTy, Int32Ty, MPtrTy,
                                              Op2->getType(), instId->getType(), Int32Ty);
          }
          if (Op2Call) {
            Instruction *Next = getNextInstruction(FCI, &BB);
            IRBuilder<> IRBI(Next);
            IRBI.CreateCall(FuncInit, {Idx, BOGEP, Op2, instId, lineNumber});
          } else
            IRB.CreateCall(FuncInit, {Idx, BOGEP, Op2, instId, lineNumber});
          ConsMap.insert(std::pair<Value *, Value *>(Op2, BOGEP));

          FuncInit =
              M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
          IRBE.CreateCall(FuncInit, {BOGEP});

          index++;
        }
      } else if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        Value *Addr = LI->getPointerOperand();
        bool BTFlag = false;

        if (BitCastInst *BI = dyn_cast<BitCastInst>(Addr)) {
          //BTFlag = checkIfBitcastFromFP(BI);
        }
        if (isFloatType(LI->getType()) || BTFlag) {
          if (index - 1 > TotalAlloca) {
            errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                   << TotalAlloca << "\n";
          }
          Value *Indices[] = {
              ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
              ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
          Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
          GEPMap.insert(std::pair<Instruction *, Value *>(&I, BOGEP));

          FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty,
                                            MPtrTy);
          IRB.CreateCall(FuncInit, {Idx, BOGEP});

          FuncInit =
              M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
          IRBE.CreateCall(FuncInit, {BOGEP});

          index++;
        }
      } else if (CallInst *CI = dyn_cast<CallInst>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        if (isFloatType(CI->getType())) {
          if (index - 1 > TotalAlloca) {
            errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
              << TotalAlloca << "\n";
          }
          Value *Indices[] = {
            ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
            ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
          Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");

          GEPMap.insert(std::pair<Instruction *, Value *>(&I, BOGEP));

          FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
              Int32Ty, MPtrTy);
          IRB.CreateCall(FuncInit, {Idx, BOGEP});

          FuncInit =
            M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
          IRBE.CreateCall(FuncInit, {BOGEP});

          index++;
        }
        size_t NumOperands = CI->getNumArgOperands();
        Value *Op[NumOperands];
        Type *OpTy[NumOperands];
        bool Op1Call[NumOperands];
        for (int i = 0; i < NumOperands; i++) {
          Op[i] = CI->getArgOperand(i);
          OpTy[i] = Op[i]->getType(); // this should be of float
          Op1Call[i] = false;

          // handle function call which take as operand another function call,
          // but that function is not defined. It then should be treated a
          // constant.
          if (isa<ConstantFP>(Op[i])) {
            if (index - 1 > TotalAlloca) {
              errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                << TotalAlloca << "\n";
            }
            Value *Indices[] = {
              ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
              ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
            Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");

            GEPMap.insert(std::pair<Instruction *, Value *>(
                  dyn_cast<Instruction>(Op[i]), BOGEP));

            FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
                Int32Ty, MPtrTy);
            IRB.CreateCall(FuncInit, {Idx, BOGEP});

            if (isFloat(Op[i]->getType())) {

              FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst_val",
                  VoidTy, Int32Ty, MPtrTy,
                  OpTy[i], instId->getType(), Int32Ty);
            } else if (isDouble(Op[i]->getType())) {

              FuncInit = M->getOrInsertFunction(
                  "fpsanx_store_tempmeta_dconst_val", VoidTy, Int32Ty, MPtrTy,
                  OpTy[i], instId->getType(), Int32Ty);
            }

            IRB.CreateCall(FuncInit, {Idx, BOGEP, Op[i], instId, lineNumber});
            ConsMap.insert(std::pair<Value *, Value *>(Op[i], BOGEP));

            FuncInit =
              M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
            IRBE.CreateCall(FuncInit, {BOGEP});

            index++;
          }
        }
      } else if (FCmpInst *FCI = dyn_cast<FCmpInst>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        if (isFloatType(I.getType())) {
          if (isa<ConstantFP>((FCI->getOperand(0)))) {
            if (index - 1 > TotalAlloca) {
              errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                     << TotalAlloca << "\n";
            }
            Value *Indices[] = {
                ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
                ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
            Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");

            GEPMap.insert(std::pair<Instruction *, Value *>(
                dyn_cast<Instruction>(FCI->getOperand(0)), BOGEP));

            FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
                                              Int32Ty, MPtrTy);
            IRB.CreateCall(FuncInit, {Idx, BOGEP});

            if (isFloat(FCI->getOperand(0)->getType())) {

              FuncInit = M->getOrInsertFunction(
                  "fpsanx_store_tempmeta_fconst_val", VoidTy, Int32Ty, MPtrTy,
                  FCI->getOperand(0)->getType(), instId->getType(), Int32Ty);

            } else if (isDouble(FCI->getOperand(0)->getType())) {

              FuncInit = M->getOrInsertFunction(
                  "fpsanx_store_tempmeta_dconst_val", VoidTy, Int32Ty, MPtrTy,
                  FCI->getOperand(0)->getType(), instId->getType(), Int32Ty);
            }
            IRB.CreateCall(FuncInit,
                           {Idx, BOGEP, FCI->getOperand(0), instId, lineNumber});
            ConsMap.insert(
                std::pair<Value *, Value *>(FCI->getOperand(0), BOGEP));

            FuncInit =
                M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
            IRBE.CreateCall(FuncInit, {BOGEP});
            index++;
          }
          if (isa<ConstantFP>((FCI->getOperand(1)))) {
            if (index - 1 > TotalAlloca) {
              errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                     << TotalAlloca << "\n";
            }
            Value *Indices[] = {
                ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
                ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
            Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");

            GEPMap.insert(std::pair<Instruction *, Value *>(
                dyn_cast<Instruction>(FCI->getOperand(1)), BOGEP));

            FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
                                              Int32Ty, MPtrTy);
            IRB.CreateCall(FuncInit, {Idx, BOGEP});

            if (isFloat(FCI->getOperand(1)->getType())) {
              FuncInit = M->getOrInsertFunction(
                  "fpsanx_store_tempmeta_fconst_val", VoidTy, Int32Ty, MPtrTy,
                  FCI->getOperand(1)->getType(), instId->getType(), Int32Ty);
            } else if (isDouble(FCI->getOperand(1)->getType())) {
              FuncInit = M->getOrInsertFunction(
                  "fpsanx_store_tempmeta_dconst_val", VoidTy, Int32Ty, MPtrTy,
                  FCI->getOperand(1)->getType(), instId->getType(), Int32Ty);
            }
            IRB.CreateCall(FuncInit,
                           {Idx, BOGEP, FCI->getOperand(1), instId, lineNumber});
            ConsMap.insert(
                std::pair<Value *, Value *>(FCI->getOperand(1), BOGEP));

            FuncInit =
                M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
            IRBE.CreateCall(FuncInit, {BOGEP});

            index++;
          }
        }
      } else if (SelectInst *SI = dyn_cast<SelectInst>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        if (isFloatType(I.getType())) {
          if (isa<ConstantFP>((SI->getOperand(1)))) {
            if (index - 1 > TotalAlloca) {
              errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                     << TotalAlloca << "\n";
            }
            Value *Indices[] = {
                ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
                ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
            Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");

            GEPMap.insert(std::pair<Instruction *, Value *>(
                dyn_cast<Instruction>(SI->getOperand(1)), BOGEP));

            FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
                                              Int32Ty, MPtrTy);
            IRB.CreateCall(FuncInit, {Idx, BOGEP});

            if (isFloat(SI->getOperand(1)->getType())) {

              FuncInit = M->getOrInsertFunction(
                  "fpsanx_store_tempmeta_fconst_val", VoidTy, Int32Ty, MPtrTy,
                  SI->getOperand(1)->getType(), instId->getType(), Int32Ty);
            } else if (isDouble(SI->getOperand(1)->getType())) {

              FuncInit = M->getOrInsertFunction(
                  "fpsanx_store_tempmeta_dconst_val", VoidTy, Int32Ty, MPtrTy,
                  SI->getOperand(1)->getType(), instId->getType(), Int32Ty);
            }
            IRB.CreateCall(FuncInit,
                           {Idx, BOGEP, SI->getOperand(1), instId, lineNumber});
            ConsMap.insert(
                std::pair<Value *, Value *>(SI->getOperand(1), BOGEP));

            FuncInit =
                M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
            IRBE.CreateCall(FuncInit, {BOGEP});
            index++;
          }
          if (isa<ConstantFP>((SI->getOperand(2)))) {
            if (index - 1 > TotalAlloca) {
              errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                     << TotalAlloca << "\n";
            }
            Value *Indices[] = {
                ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
                ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
            Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");

            GEPMap.insert(std::pair<Instruction *, Value *>(
                dyn_cast<Instruction>(SI->getOperand(2)), BOGEP));

            FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
                                              Int32Ty, MPtrTy);
            IRB.CreateCall(FuncInit, {Idx, BOGEP});

            if (isFloat(SI->getOperand(2)->getType())) {

              FuncInit = M->getOrInsertFunction(
                  "fpsanx_store_tempmeta_fconst_val", VoidTy, Int32Ty, MPtrTy,
                  SI->getOperand(2)->getType(), instId->getType(), Int32Ty);
            } else if (isDouble(SI->getOperand(2)->getType())) {

              FuncInit = M->getOrInsertFunction(
                  "fpsanx_store_tempmeta_dconst_val", VoidTy, Int32Ty, MPtrTy,
                  SI->getOperand(2)->getType(), instId->getType(), Int32Ty);
            }
            IRB.CreateCall(FuncInit,
                           {Idx, BOGEP, SI->getOperand(2), instId, lineNumber});
            ConsMap.insert(
                std::pair<Value *, Value *>(SI->getOperand(2), BOGEP));

            FuncInit =
                M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
            IRBE.CreateCall(FuncInit, {BOGEP});

            index++;
          }
        }
      } else if (PHINode *PN = dyn_cast<PHINode>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        if (isFloatType(I.getType())) {
          if (index - 1 > TotalAlloca) {
            errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                   << TotalAlloca << "\n";
          }

          Value *Indices[] = {
              ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
              ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
          Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");

          GEPMap.insert(std::pair<Instruction *, Value *>(&I, BOGEP));

          FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty,
                                            MPtrTy);
          IRB.CreateCall(FuncInit, {Idx, BOGEP});

          FuncInit =
              M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
          IRBE.CreateCall(FuncInit, {BOGEP});

          index++;
        }
        for (unsigned PI = 0, PE = PN->getNumIncomingValues(); PI != PE; ++PI) {
          Value *IncValue = PN->getIncomingValue(PI);

          if (IncValue == PN)
            continue; 
          if (isa<ConstantFP>(IncValue)) {
            if (index - 1 > TotalAlloca) {
              errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                     << TotalAlloca << "\n";
            }
            Value *Indices[] = {
                ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
                ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
            Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");

            GEPMap.insert(std::pair<Instruction *, Value *>(
                dyn_cast<Instruction>(IncValue), BOGEP));

            FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
                                              Int32Ty, MPtrTy);
            IRB.CreateCall(FuncInit, {Idx, BOGEP});

            if (isFloat(IncValue->getType())) {

              FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst_val",
                                                VoidTy, Int32Ty, MPtrTy,
                                                IncValue->getType(), instId->getType(), Int32Ty);
            } else if (isDouble(IncValue->getType())) {

              FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_dconst_val",
                                                VoidTy, Int32Ty, MPtrTy,
                                                IncValue->getType(), instId->getType(), Int32Ty);
            }
            IRB.CreateCall(FuncInit, {Idx, BOGEP, IncValue, instId, lineNumber});
            ConsMap.insert(std::pair<Value *, Value *>(IncValue, BOGEP));

            FuncInit =
                M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
            IRBE.CreateCall(FuncInit, {BOGEP});

            index++;
          }
        }
      }
      if (ReturnInst *RT = dyn_cast<ReturnInst>(&I)) {
        if (GEPMap.count(&I) != 0) {
          continue;
        }
        if (RT->getNumOperands() != 0) {
          Value *Op = RT->getOperand(0);
          if (isFloatType(Op->getType())) {
            if (isa<ConstantFP>(Op)) {
              if (index - 1 > TotalAlloca) {
                errs() << "Error:\n\n\n index > TotalAlloca " << index << ":"
                       << TotalAlloca << "\n";
              }
              Value *Indices[] = {
                  ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
                  ConstantInt::get(Type::getInt32Ty(M->getContext()), index)};
              Value *BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");

              GEPMap.insert(std::pair<Instruction *, Value *>(
                  dyn_cast<Instruction>(Op), BOGEP));

              FuncInit = M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy,
                                                Int32Ty, MPtrTy);
              IRB.CreateCall(FuncInit, {Idx, BOGEP});

              if (isFloat(Op->getType())) {
                FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_fconst_val",
                                                  VoidTy, Int32Ty, MPtrTy,
                                                  Op->getType(), instId->getType(), Int32Ty);
              } else if (isDouble(Op->getType())) {
                FuncInit = M->getOrInsertFunction("fpsanx_store_tempmeta_dconst_val", 
                                                  VoidTy, Int32Ty, MPtrTy,
                                                  Op->getType(), instId->getType(), Int32Ty);
              }
              IRB.CreateCall(FuncInit, {Idx, BOGEP, Op, instId, lineNumber});
              ConsMap.insert(std::pair<Value *, Value *>(Op, BOGEP));

              FuncInit =
                  M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
              IRBE.CreateCall(FuncInit, {BOGEP});

              index++;
            }
          }
        }
      }
    }
  }
}

AllocaInst *FPSanitizer::createAlloca(Function *F, size_t InsCount) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First;
  for (auto &I : BB) {
    if (CallInst *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (Callee) {
        if (Callee->getName().startswith("fpsanx_get") ||
            Callee->getName().startswith("start_slice")) {
          First = &I;
        }
      }
    }
  }
  Instruction *Next = getNextInstruction(First, &BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  Instruction *End;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (dyn_cast<ReturnInst>(&I)) {
        End = &I;
      }
    }
  }

  AllocaInst *Alloca =
      IRB.CreateAlloca(ArrayType::get(Real, InsCount), nullptr, "my_alloca");
  return Alloca;
}

long FPSanitizer::getTotalFPInst(Function *F) {
  long TotalAlloca = 0;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (UnaryOperator *UO = dyn_cast<UnaryOperator>(&I)) {
        switch (UO->getOpcode()) {
        case Instruction::FNeg: {
          TotalAlloca++;
        }
        }
      }
      if (BinaryOperator *BO = dyn_cast<BinaryOperator>(&I)) {
        switch (BO->getOpcode()) {
        case Instruction::FAdd:
        case Instruction::FSub:
        case Instruction::FMul:
        case Instruction::FDiv: {
          Value *Op1 = BO->getOperand(0);
          Value *Op2 = BO->getOperand(1);

          TotalAlloca++;

          if (isa<ConstantFP>(Op1)) {
            TotalAlloca++;
          }
          if (isa<ConstantFP>(Op2)) {
            TotalAlloca++;
          }
        }
        }
      } else if (SIToFPInst *UI = dyn_cast<SIToFPInst>(&I)) {
        TotalAlloca++;
      } else if (BitCastInst *UI = dyn_cast<BitCastInst>(&I)) {
        if (isFloatType(UI->getType())) {
          TotalAlloca++;
        }
      } else if (UIToFPInst *UI = dyn_cast<UIToFPInst>(&I)) {
        TotalAlloca++;
      } else if (FCmpInst *FCI = dyn_cast<FCmpInst>(&I)) {
        Value *Op1 = FCI->getOperand(0);
        Value *Op2 = FCI->getOperand(1);

        bool Op1Call = false;
        bool Op2Call = false;

        if (CallInst *CI = dyn_cast<CallInst>(Op1)) {
          Function *Callee = CI->getCalledFunction();
          if (Callee) {
            if (!isListedFunction(Callee->getName(), "mathFunc.txt")) {
              // this operand is function which is not defined, we consider this
              // as a constant
              Op1Call = true;
            }
          }
        }
        if (CallInst *CI = dyn_cast<CallInst>(Op2)) {
          Function *Callee = CI->getCalledFunction();
          if (Callee) {
            if (!isListedFunction(Callee->getName(), "mathFunc.txt")) {
              Op2Call = true;
            }
          }
        }

        if (isa<ConstantFP>(Op1) || Op1Call) {
          TotalAlloca++;
        }
        if (isa<ConstantFP>(Op2) || Op2Call) {
          TotalAlloca++;
        }
      } else if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
        if (isFloatType(LI->getType())) {
          TotalAlloca++;
        }
      } else if (CallInst *CI = dyn_cast<CallInst>(&I)) {
        Function *Callee = CI->getCalledFunction();
        if (isFloatType(CI->getType())) {
          TotalAlloca++;
        }
        size_t NumOperands = CI->getNumArgOperands();
        Value *Op[NumOperands];
        Type *OpTy[NumOperands];
        bool Op1Call[NumOperands];
        for (int i = 0; i < NumOperands; i++) {
          Op[i] = CI->getArgOperand(i);
          OpTy[i] = Op[i]->getType(); // this should be of float

          if (isa<ConstantFP>(Op[i])) {
            TotalAlloca++;
          }
        }
      } else if (FCmpInst *FCI = dyn_cast<FCmpInst>(&I)) {
        if (isFloatType(I.getType())) {
          if (isa<ConstantFP>((FCI->getOperand(0)))) {
            TotalAlloca++;
          }
          if (isa<ConstantFP>((FCI->getOperand(1)))) {
            TotalAlloca++;
          }
        }
      } else if (SelectInst *SI = dyn_cast<SelectInst>(&I)) {
        if (isFloatType(I.getType())) {
          if (isa<ConstantFP>((SI->getOperand(1)))) {
            TotalAlloca++;
          }
          if (isa<ConstantFP>((SI->getOperand(2)))) {
            TotalAlloca++;
          }
        }
      } else if (PHINode *PN = dyn_cast<PHINode>(&I)) {
        if (isFloatType(I.getType())) {
          TotalAlloca++;
        }
        for (unsigned PI = 0, PE = PN->getNumIncomingValues(); PI != PE; ++PI) {
          Value *IncValue = PN->getIncomingValue(PI);

          if (IncValue == PN)
            continue; // TODO
          if (isa<ConstantFP>(IncValue))
            if (isa<ConstantFP>(IncValue)) {
              TotalAlloca++;
            }
        }
      } else if (ReturnInst *RT = dyn_cast<ReturnInst>(&I)) {
        if (RT->getNumOperands() != 0) {
          Value *Op = RT->getOperand(0);
          if (isFloatType(Op->getType())) {
            if (isa<ConstantFP>(Op)) {
              TotalAlloca++;
            }
          }
        }
      }
    }
  }
  return TotalAlloca;
}

void FPSanitizer::createMpfrAlloca(Function *F) {
  long TotalArg = 1;
  long TotalAlloca = 0;
  for (Function::arg_iterator ait = F->arg_begin(), aend = F->arg_end();
       ait != aend; ++ait) {
    Argument *A = &*ait;
    ArgMap.insert(std::pair<Argument *, long>(A, TotalArg));
    TotalArg++;
  }

  FuncTotalArg.insert(std::pair<Function *, long>(F, TotalArg));
  TotalArg = 1;
  TotalAlloca = getTotalFPInst(F);
  AllocaInst *Alloca;
  if (TotalAlloca > 0) {
    Alloca = createAlloca(F, TotalAlloca);
    createGEP(F, Alloca, TotalAlloca);
  }
  TotalAlloca = 0;
}

Instruction *FPSanitizer::getNextInstruction(Instruction *I, BasicBlock *BB) {
  Instruction *Next;
  for (BasicBlock::iterator BBit = BB->begin(), BBend = BB->end();
       BBit != BBend; ++BBit) {
    Next = &*BBit;
    if (I == Next) {
      Next = &*(++BBit);
      break;
    }
  }
  return Next;
}

Instruction *FPSanitizer::getNextInstructionNotPhi(Instruction *I,
                                                   BasicBlock *BB) {
  Instruction *Next;
  for (auto &I : *BB) {
    if (!isa<PHINode>(I)) {
      Next = &I;
      break;
    }
  }
  return Next;
}

void FPSanitizer::findInterestingFunctions(Function *F) {

  long TotalFPInst = getTotalFPInst(F);
  if (TotalFPInst > 0) {
    std::string name = F->getName();
    addFunctionsToList(name);
  }
}

void FPSanitizer::handleFuncShadowMainInit(Function *F) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;

  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  Instruction *Last;
  for (BasicBlock &BB : *F) {
    if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      Last = RI;
    }
  }
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IRBuilder<> IRB(Last);
  Value *Idx = BufIdxMap.at(F);
  Finish = M->getOrInsertFunction("fpsan_finish_shadow", VoidTy, Int32Ty);
//  IRB.CreateCall(Finish, {Idx});
}

void FPSanitizer::handleFuncMainInit(Function *F) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;

  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  IRBuilder<> IRB(First);
  Finish = M->getOrInsertFunction("fpsan_init", VoidTy);
  IRB.CreateCall(Finish, {});
}

void FPSanitizer::handleStartSliceC(Function *F) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;

  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  Instruction *Last;
  for (BasicBlock &BB : *F) {
    if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      Last = RI;
    }
  }
  Argument *A;
  for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E;
       ++I) {
    A = &*I;
  }
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IRBuilder<> IRB(First);
  Finish = M->getOrInsertFunction("fpsan_shadow_slice_start", VoidTy, A->getType());
  IRB.CreateCall(Finish, {A});
}

void FPSanitizer::handleEndSliceC(Function *F) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;

  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  Instruction *Last;
  for (BasicBlock &BB : *F) {
    if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      Last = RI;
    }
  }
  Argument *A;
  for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E;
       ++I) {
    A = &*I;
  }
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  IRBuilder<> IRB(First);
  Finish =
      M->getOrInsertFunction("fpsan_shadow_slice_end", VoidTy, A->getType());
  IRB.CreateCall(Finish, {A});
  IRB.CreateStore(ConstantInt::get(Int64Ty, 0), CQIdx, "my_store_idx");
}

void FPSanitizer::handleStartSliceCallInstP(CallInst *CI, Function *F) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;

  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  IntegerType *Int8Ty = Type::getInt8Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));
  Type *PtrDoubleTy =
      PointerType::getUnqual(Type::getDoubleTy(M->getContext()));

  for (auto &I : BB) {
    if (CallInst *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (Callee && (Callee->getName() == "fpsanx_get_buf_idx")) {
        First = &I;
      }
    }
  }
  IRBuilder<> IRB(First->getNextNode());

  IRB.CreateStore(ConstantInt::get(Int8Ty, 1), SliceFlag, "my_slice_flag");
  Finish = M->getOrInsertFunction("fpsan_slice_start", VoidTy, Int32Ty);
//  IRB.CreateCall(Finish, {Idx});
}

void FPSanitizer::handleEndSliceCallInstP(CallInst *CI, Function *F) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;

  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  IntegerType *Int8Ty = Type::getInt8Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  IRBuilder<> IRB(CI);

  Function *ShadowFunc = CloneFuncMap.at(F);
  BitCastInst *BCToAddr = new BitCastInst(
      ShadowFunc, PointerType::getUnqual(Type::getInt8Ty(M->getContext())), "",
      CI);

  //reset buf idx
  IRB.CreateStore(ConstantInt::get(Int8Ty, 0), SliceFlag, "my_slice_flag");
  IRB.CreateStore(ConstantInt::get(Int64Ty, 0), QIdx, "my_store_idx");

  Finish = M->getOrInsertFunction("fpsan_slice_end", VoidTy, PtrVoidTy);
  IRB.CreateCall(Finish, {BCToAddr});
}

void FPSanitizer::handleMainRet(Function *F) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;

  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  Instruction *Last;
  for (BasicBlock &BB : *F) {
    if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
      Last = RI;
    }
  }
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IRBuilder<> IRB(Last);
  Finish = M->getOrInsertFunction("fpsan_finish", VoidTy);
  IRB.CreateCall(Finish, {});
}

void FPSanitizer::handlePrint(CallInst *CI, BasicBlock *BB, Function *F) {
  Instruction *I = dyn_cast<Instruction>(CI);
  Instruction *Next = getNextInstruction(dyn_cast<Instruction>(CI), BB);
  IRBuilder<> IRB(Next);

  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());
  size_t NumOperands = CI->getNumArgOperands();
  Value *Op[NumOperands];
  Type *OpTy[NumOperands];
  Value *OpIdx = ConstantPointerNull::get(cast<PointerType>(MPtrTy));
  for (int i = 0; i < NumOperands; i++) {
    Op[i] = CI->getArgOperand(i);
    OpTy[i] = Op[i]->getType(); // this should be of float
    if (isFloatType(OpTy[i]) || OpTy[i] == MPtrTy) {
      Instruction *OpIns = dyn_cast<Instruction>(Op[i]);
      bool res = handleOperand(I, Op[i], F, &OpIdx);
      if (!res) {
        errs() << "\nhandlePrint Error !!! metadata not found for operand:\n";
        errs() << "In Inst:"
               << "\n";
      }
      // if operand is a constant, then we just skip printing it
      else {
//        FuncInit = M->getOrInsertFunction("fpsan_print_real", VoidTy, MPtrTy);
 //       IRB.CreateCall(FuncInit, {OpIdx});
      }
    }
  }
}

void FPSanitizer::handleCallInstProducer(CallInst *CI, BasicBlock *BasicB, Function *F) {
  Function *Callee = CI->getCalledFunction();
  Instruction *Next = getNextInstruction(dyn_cast<Instruction>(CI), BasicB);
  if (LibFuncList.count(Callee->getName()) != 0) {
    return;
  }
  Instruction *I = dyn_cast<Instruction>(CI);
  if (std::find(AllBranchList.begin(), AllBranchList.end(), I) != AllBranchList.end()) {
    return;
  }
  Module *M = F->getParent();
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;
  IRBuilder<> IRBB(First);
  IRBuilder<> IRB(I);
  IRBuilder<> IRBN(Next);

  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  // push args to buf
  size_t NumOperands = CI->getNumArgOperands();
  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  // We want to push arguments only we called function is a slice, otherwise arguments will be propagated
  // via temporary entry. 
  if (isListedFunction(Callee->getName(), "libFunc.txt")) {
    Value *BufAddr = BufAddrMap.at(F);
    Value *DFPVal;
    if(isDouble(CI->getType())){
      DFPVal = CI;
    }
    else if(isFloat(CI->getType())){
      DFPVal = IRBN.CreateFPExt(CI, Type::getDoubleTy(M->getContext()));
    }
    BasicBlock *OldBB = I->getParent();
    BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
    createUpdateBlock(DFPVal, BufAddr, I, OldBB, Cont, F);
  }
}

void FPSanitizer::handleICallInstProducer(CallInst *CI, BasicBlock *BasicB, Function *F) {
  if (F->getName().startswith("start_slice") ||
      F->getName().startswith("end_slice")) {
    return;
  }
  Function *Callee = CI->getCalledFunction();
  Instruction *I = dyn_cast<Instruction>(CI);
  Module *M = F->getParent();
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;
  IRBuilder<> IRBB(First);
  IRBuilder<> IRB(I);

  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  // push args to buf
  size_t NumOperands = CI->getNumArgOperands();
  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  bool flag = false;
  if (std::find(SliceList.begin(), SliceList.end(), Callee) != SliceList.end()) {
    for (int i = 0; i < NumOperands; i++) {
      Value *Op = CI->getArgOperand(i);
      if(isFloatType(Op->getType())){ 
        flag = true;
      }
    }
  }
  BasicBlock *OldBB, *Cont;

  //push func addr
  BitCastInst *BCToAddr = new BitCastInst(
      CI->getCalledOperand(), PointerType::getUnqual(Type::getInt8Ty(M->getContext())), "",
      I);
  FuncInit = M->getOrInsertFunction("fpsanx_push_addr", VoidTy, Int32Ty,
      PtrVoidTy);
  Value *BufAddr = BufAddrMap.at(F);
  Value *PtrToInt = IRB.CreatePtrToInt(CI->getCalledOperand(), Int64Ty, "my_ptr_int");
  Value *FPVal = IRB.CreateUIToFP(PtrToInt,  Type::getDoubleTy(M->getContext()), "my_si_fp");
  OldBB = I->getParent();
  Cont = OldBB->splitBasicBlock(CI, "split");
  createUpdateBlock(FPVal, BufAddr, I, OldBB, Cont, F);
}


void FPSanitizer::handleCallInstIndirect(CallInst *CI, BasicBlock *BB, Function *F) {
  Function *Callee = CI->getCalledFunction();
  Instruction *I = dyn_cast<Instruction>(CI);
  BasicBlock::iterator BBit = BB->begin();
  Instruction *First = &*BBit;
  IRBuilder<> IRBB(First);
  IRBuilder<> IRB(I);
  Module *M = F->getParent();
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));
  std::vector<Value *> Args;
  std::vector<Type *> ArgsTy;
  size_t NumOperands = CI->getNumArgOperands();
  Value *Op[NumOperands];
  Type *OpTy[NumOperands];
  Type *VoidTy = Type::getVoidTy(M->getContext());
  Value *OpIdx = ConstantPointerNull::get(cast<PointerType>(MPtrTy));
  for (int i = 0; i < NumOperands; i++) {
    Op[i] = CI->getArgOperand(i);
    OpTy[i] = Op[i]->getType(); // this should be of float
    if (isFloatType(OpTy[i]) || OpTy[i] == MPtrTy) {
      Instruction *OpIns = dyn_cast<Instruction>(Op[i]);
      bool res = handleOperand(I, Op[i], F, &OpIdx);
      if (!res) {
        errs()<< "\nhandleCallInstIndirect Error !!! metadata not found for operand:"<<F->getName()<<"\n";
        errs()<<*Op[i]<<"\n";
        errs() << "In Inst:"
               << "\n";
        errs()<<*I<<"\n";
        exit(1);
      }
      Args.push_back(OpIdx);
      ArgsTy.push_back(OpIdx->getType());
    }
  }
  // Check return type
  if (isFloatType(CI->getType())) {
    if (GEPMap.count(CI) == 0) {
      errs() << "Add this function...:" << *Callee << "\n";
      exit(1);
    }
    Value *GEP = GEPMap.at(CI);
    Args.push_back(GEP);
    ArgsTy.push_back(GEP->getType());
    MInsMap.insert(std::pair<Instruction *, Instruction *>(
          CI, dyn_cast<Instruction>(GEP)));
  }

  if(BufIdxMap.count(F) > 0){
    //get func addr
    Value *Idx = BufIdxMap.at(F);
    
    FuncInit = M->getOrInsertFunction("fpsanx_get_clone_addr", PtrVoidTy, Int32Ty, Int64Ty);
    Value *ToAddr = readFromBuf(I, BB, F);
    Value *ToAddrInt = IRB.CreateFPToUI(ToAddr,  Type::getInt64Ty(M->getContext()), "my_si_fp");
    Value *CloneAddr = IRB.CreateCall(FuncInit, {Idx, ToAddrInt}, "my_func");
    FunctionType *NFTy = FunctionType::get(VoidTy, ArgsTy, false);
    BitCastInst *FAddr = new BitCastInst(
        CloneAddr, PointerType::getUnqual(NFTy), "my_bitcast", I);
    // Create the new function body and insert it into the module...
    Function *NewFn = Function::Create(NFTy, F->getLinkage(), F->getAddressSpace());
    CallSite NewCS;
    if (std::find(SliceList.begin(), SliceList.end(), Callee) ==
        SliceList.end()) {
      NewCS = CallInst::Create(NFTy, FAddr, Args, "", CI);
    }
    AllInstList.push_back(CI);
  }
}

void FPSanitizer::handleCallInst(CallInst *CI, BasicBlock *BB, Function *F) {
  Function *Callee = CI->getCalledFunction();
  if (Callee && LibFuncList.count(Callee->getName()) != 0) {
    return;
  }
  if(Callee->getName().startswith("pull_print")){
    return;
  }
  Instruction *I = dyn_cast<Instruction>(CI);
  Instruction *Next = getNextInstruction(dyn_cast<Instruction>(CI), BB);
  IRBuilder<> IRB(I);
  Module *M = F->getParent();
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));
  Type *VoidTy = Type::getVoidTy(M->getContext());

  std::vector<Value *> Args;
  size_t NumOperands = CI->getNumArgOperands();
  Value *Op[NumOperands];
  Type *OpTy[NumOperands];
  Value *OpIdx = ConstantPointerNull::get(cast<PointerType>(MPtrTy));
  for (int i = 0; i < NumOperands; i++) {
    Op[i] = CI->getArgOperand(i);
    OpTy[i] = Op[i]->getType(); // this should be of float
    if (isFloatType(OpTy[i]) || OpTy[i] == MPtrTy) {
      Instruction *OpIns = dyn_cast<Instruction>(Op[i]);
      bool res = handleOperand(I, Op[i], F, &OpIdx);
      if (!res) {
        errs()<< "\nhandleCallInst Error !!! metadata not found for operand:"<<F->getName()<<"\n";
        errs()<<*Op[i]<<"\n";
        errs() << "In Inst:"
               << "\n";
        errs()<<*I<<"\n";
        exit(1);
      }
      Args.push_back(OpIdx);
    }
  }
  // Check return type
  if (isFloatType(CI->getType())) {
    if (LibFuncList.count(Callee->getName()) == 0) {
      if (GEPMap.count(CI) == 0) {
        errs() << "Add this function...:" << Callee->getName() << "\n";
        exit(1);
      }
      Value *GEP = GEPMap.at(CI);
      Args.push_back(GEP);
      MInsMap.insert(std::pair<Instruction *, Instruction *>(
          CI, dyn_cast<Instruction>(GEP)));
    }
  }
  

  // replace the call instruction with the shadow function call instruction
  if (CloneFuncMap.count(Callee) != 0) {
    Function *ShadowFunc = CloneFuncMap.at(Callee);
    CallSite NewCS;
    if (std::find(SliceList.begin(), SliceList.end(), Callee) ==
        SliceList.end()) {
      NewCS = CallInst::Create(ShadowFunc, Args, "", CI);
    }
    AllInstList.push_back(CI);
  }
  else{
    //There is no clone function, hence it is a lib call 
    //Check return type and push the return value to buf
    if (isListedFunction(Callee->getName(), "libFunc.txt")) {
      ConstantInt *instId = GetInstId(F, I);
      Value *Idx = BufIdxMap.at(F);
      Value *BOGEP = GEPMap.at(I);

      Value *Val = readFromBuf(I, BB, F);
      FuncInit = M->getOrInsertFunction("fpsan_set_lib", VoidTy, Int32Ty, 
                                      Type::getDoubleTy(M->getContext()),
                                      MPtrTy, instId->getType());
      IRB.CreateCall(FuncInit, {Idx, Val, BOGEP, instId});
    }
  }
}

void FPSanitizer::handleFuncInit(Function *F) {
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;

  for (auto &I : BB) {
    if (CallInst *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (Callee && (Callee->getName() == "fpsanx_get_buf_idx_c")) {
        First = &I;
      }
    }
  }
  for (auto &I : BB) {
    if (CallInst *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (Callee && (Callee->getName().startswith("start_slice"))) {
        First = &I;
      }
    }
  }
  Module *M = F->getParent();
  IRBuilder<> IRB(First->getNextNode());
  Type *VoidTy = Type::getVoidTy(M->getContext());
  Type *Int8Ty = Type::getInt8Ty(M->getContext());
  Type *Int32Ty = Type::getInt32Ty(M->getContext());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());

  FuncInit = M->getOrInsertFunction("fpsan_func_init", VoidTy, Int32Ty);
  if (BufIdxMap.count(F) > 0) {
    Value *Idx = BufIdxMap.at(F);
    IRB.CreateCall(FuncInit, {Idx});
  }
}

void FPSanitizer::handleMathLibFunc(CallInst *CI, BasicBlock *BB, Function *F,
                                    std::string CallName) {

  Instruction *I = dyn_cast<Instruction>(CI);
  Instruction *Next = getNextInstruction(dyn_cast<Instruction>(CI), BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());

  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());

  SmallVector<Type *, 4> ArgsTy;
  SmallVector<Value *, 8> ArgsVal;

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);

  Value *BOGEP = GEPMap.at(I);

  Value *Index1;

  size_t NumOperands = CI->getNumArgOperands();
  Value *Op[NumOperands];
  Type *OpTy[NumOperands];
  bool Op1Call[NumOperands];
  Value *ConsIdx[NumOperands];

  std::string funcName;

  if (CallName == "llvm.cos.f64") {
    funcName = "fpsan_mpfr_llvm_cos_f64";
  }
  else if (CallName == "llvm.fma.f64") {
    funcName = "fpsan_mpfr_llvm_fma_f64";
  }
  else if (CallName == "llvm.sin.f64") {
    funcName = "fpsan_mpfr_llvm_sin_f64";
  }
  else if (CallName == "llvm.ceil.f64") {
    funcName = "fpsan_mpfr_llvm_ceil";
  } else if (CallName == "llvm.floor.f64") {
    funcName = "fpsan_mpfr_llvm_floor_d";
  } else if (CallName == "llvm.floor.f32") {
    funcName = "fpsan_mpfr_llvm_floor_f";
  } else if (CallName == "llvm.fabs.f32") {
    funcName = "fpsan_mpfr_llvm_fabs";
  } else if (CallName == "llvm.fabs.f64") {
    funcName = "fpsan_mpfr_llvm_fabs";
  } else {
    funcName = "fpsan_mpfr_" + CallName;
  }

  Value *Idx = BufIdxMap.at(F);
  // thearid
  ArgsTy.push_back(Int32Ty);
  ArgsVal.push_back(Idx);
  for (int i = 0; i < NumOperands; i++) {
    Op[i] = CI->getArgOperand(i);
    OpTy[i] = Op[i]->getType(); // this should be of float
    Op1Call[i] = false;
    bool res = handleOperand(I, Op[i], F, &ConsIdx[i]);
    if (!res) {
      errs() << "\nhandleMathLibFunc Error !!! metadata not found for "
        "operand in func:"
        << F->getName() << ":\n";
      errs() << *Op[i] << "\n";
      errs() << "In Inst:"
        << "\n";
      errs() << *I << "\n";
      exit(1);
    }
    ArgsVal.push_back(ConsIdx[i]);
    ArgsTy.push_back(MPtrTy);
  }
  ArgsTy.push_back(MPtrTy);
  ArgsTy.push_back(Int64Ty);
  ArgsTy.push_back(Int32Ty);

  ArgsVal.push_back(BOGEP);

  if (NumOperands > 1){
    funcName = funcName + std::to_string(NumOperands);
  }

  MInsMap.insert(std::pair<Instruction *, Instruction *>(I, dyn_cast<Instruction>(BOGEP)));

  HandleFunc = M->getOrInsertFunction(
      funcName, FunctionType::get(IRB.getVoidTy(), ArgsTy, false));

  ArgsVal.push_back(instId);
  ArgsVal.push_back(lineNumber);

  IRB.CreateCall(HandleFunc, ArgsVal);
}

bool FPSanitizer::handleOperand(Instruction *I, Value *OP, Function *F,
                                Value **ConsInsIndex) {
  Module *M = F->getParent();
  long Idx = 0;

  IRBuilder<> IRB(I);
  Instruction *OpIns = dyn_cast<Instruction>(OP);
  Type *Int64Ty = Type::getInt64Ty(M->getContext());

  if (ConsMap.count(OP) != 0) {
    *ConsInsIndex = ConsMap.at(OP);
    return true;
  } else if (OP->getType() == MPtrTy) {
    *ConsInsIndex = OP;
    return true;
  }
  else if(isa<PHINode>(OP) && GEPMap.count(dyn_cast<Instruction>(OP)) != 0){
    *ConsInsIndex = GEPMap.at(dyn_cast<Instruction>(OP));
    return true;
  }
  else if (MInsMap.count(dyn_cast<Instruction>(OP)) != 0) {
    *ConsInsIndex = MInsMap.at(dyn_cast<Instruction>(OP));
    return true;
  } else if (isa<Argument>(OP)) {
    *ConsInsIndex = OP;
    return true;
  } else if (isa<FPTruncInst>(OP) || isa<FPExtInst>(OP)) {
    Value *OP1 = OpIns->getOperand(0);
    if (isa<FPTruncInst>(OP1) || isa<FPExtInst>(OP1)) {
      Value *OP2 = (dyn_cast<Instruction>(OP1))->getOperand(0);
      if (MInsMap.count(dyn_cast<Instruction>(OP2)) !=
          0) { // TODO need recursive func
        *ConsInsIndex = MInsMap.at(dyn_cast<Instruction>(OP2));
        return true;
      } else {
        return false;
      }
    } else if (MInsMap.count(dyn_cast<Instruction>(OP1)) != 0) {
      *ConsInsIndex = MInsMap.at(dyn_cast<Instruction>(OP1));
      return true;
    } else if (ConsMap.count(OP1) != 0) {
      *ConsInsIndex = ConsMap.at(OP1);
      return true;
    } else if (OP1->getType() == MPtrTy) {
      *ConsInsIndex = OP1;
      return true;
    } else {
      return false;
    }
  }
  else if(isa<UndefValue>(OP)){
    *ConsInsIndex = ConstantPointerNull::get(cast<PointerType>(MPtrTy));
    return true;
  }
  else {
    return false;
  }
}

void FPSanitizer::handleMemsetProducer(CallInst *CI, BasicBlock *BB,
                                       Function *F, std::string CallName) {

  Instruction *I = dyn_cast<Instruction>(CI);
  Instruction *Next = getNextInstruction(dyn_cast<Instruction>(CI), BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());

  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *debugInfoAvailable = ConstantInt::get(Int1Ty, debugInfoAvail);
  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);
  ConstantInt *colNumber = ConstantInt::get(Int32Ty, colNum);

  Value *Op1Addr = CI->getOperand(0);
  Value *Op2Val = CI->getOperand(1);
  Value *size = CI->getOperand(2);
  if (BitCastInst *BI = dyn_cast<BitCastInst>(Op1Addr)) {
    if (checkIfBitcastFromFP(BI)) {
      Value *BufAddr = BufAddrMap.at(F);
      Value *PtrToInt = IRB.CreatePtrToInt(Op1Addr, Int64Ty, "my_ptr_int");
      Value *FPVal = IRB.CreateUIToFP(PtrToInt,  Type::getDoubleTy(M->getContext()), "my_si_fp");

      Value *FPValSize = IRB.CreateUIToFP(size,  Type::getDoubleTy(M->getContext()), "my_si_fp");
      BasicBlock *OldBB = I->getParent();
      BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
      createUpdateBlock2(FPVal, FPValSize, BufAddr, I, OldBB, Cont, F);
    }
  }
  if (LoadInst *LI = dyn_cast<LoadInst>(Op1Addr)) {
    Value *Addr = LI->getPointerOperand();
    if (BitCastInst *BI = dyn_cast<BitCastInst>(Addr)) {
      if (checkIfBitcastFromFP(BI)) {
      Value *BufAddr = BufAddrMap.at(F);
      Value *PtrToInt = IRB.CreatePtrToInt(Op1Addr, Int64Ty, "my_ptr_int");
      Value *FPVal = IRB.CreateUIToFP(PtrToInt,  Type::getDoubleTy(M->getContext()), "my_si_fp");

      Value *FPValSize = IRB.CreateUIToFP(size,  Type::getDoubleTy(M->getContext()), "my_si_fp");
      BasicBlock *OldBB = I->getParent();
      BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
      createUpdateBlock2(FPVal, FPValSize, BufAddr, I, OldBB, Cont, F);
      }
    }
  }
}

void FPSanitizer::handleMemset(CallInst *CI, BasicBlock *BB, Function *F,
                               std::string CallName) {

  Instruction *I = dyn_cast<Instruction>(CI);
  Instruction *Next = getNextInstruction(dyn_cast<Instruction>(CI), BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());

  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  IntegerType *Int8Ty = Type::getInt8Ty(M->getContext());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *debugInfoAvailable = ConstantInt::get(Int1Ty, debugInfoAvail);
  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);
  ConstantInt *colNumber = ConstantInt::get(Int32Ty, colNum);

  Value *Op1Addr = CI->getOperand(0);
  Value *Op2Val = CI->getOperand(1);
  if (BitCastInst *BI = dyn_cast<BitCastInst>(Op1Addr)) {
    if (checkIfBitcastFromFP(BI)) {
      Value *Idx = BufIdxMap.at(F);
      FuncInit = M->getOrInsertFunction("fpsan_handle_memset", VoidTy, Int32Ty, Int64Ty, Int64Ty,
                                        Int8Ty, instId->getType());
      Value *Addr = readFromBuf(I, BB, F);
      Value *AddrInt = IRB.CreateFPToUI(Addr,  Type::getInt64Ty(M->getContext()), "my_si_fp");

      Value *Val = readFromBuf(I, BB, F);
      Value *ValInt = IRB.CreateFPToUI(Val,  Type::getInt64Ty(M->getContext()), "my_si_fp");

      IRB.CreateCall(FuncInit, {Idx, AddrInt, ValInt, Op2Val, instId});
    }
  }
  if (LoadInst *LI = dyn_cast<LoadInst>(Op1Addr)) {
    Value *Addr = LI->getPointerOperand();
    if (BitCastInst *BI = dyn_cast<BitCastInst>(Addr)) {
      if (checkIfBitcastFromFP(BI)) {
        Value *Idx = BufIdxMap.at(F);
        FuncInit = M->getOrInsertFunction("fpsan_handle_memset", VoidTy, Int32Ty, Int64Ty, Int64Ty,
                                          Int8Ty, instId->getType());
        Value *Addr = readFromBuf(I, BB, F);
        Value *AddrInt = IRB.CreateFPToUI(Addr,  Type::getInt64Ty(M->getContext()), "my_si_fp");

        Value *Val = readFromBuf(I, BB, F);
        Value *ValInt = IRB.CreateFPToUI(Val,  Type::getInt64Ty(M->getContext()), "my_si_fp");

        IRB.CreateCall(FuncInit, {Idx, AddrInt, ValInt, Op2Val, instId});
        AllInstList.push_back(LI);
      }
    }
  }
  AllInstList.push_back(CI);
}

void FPSanitizer::handleMemCpyProducer(CallInst *CI, BasicBlock *BB,
                                       Function *F, std::string CallName) {

  Instruction *I = dyn_cast<Instruction>(CI);
  Instruction *Next = getNextInstruction(dyn_cast<Instruction>(CI), BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());

  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *debugInfoAvailable = ConstantInt::get(Int1Ty, debugInfoAvail);
  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);
  ConstantInt *colNumber = ConstantInt::get(Int32Ty, colNum);

  Value *Op1Addr = CI->getOperand(0);
  Value *Op2Addr = CI->getOperand(1);
  Value *size = CI->getOperand(2);

  if (BitCastInst *BI = dyn_cast<BitCastInst>(Op1Addr)) {
    if (checkIfBitcastFromFP(BI)) {
      Value *BufAddr = BufAddrMap.at(F);
      Value *PtrToInt = IRB.CreatePtrToInt(Op1Addr, Int64Ty, "my_ptr_int");
      Value *FPVal = IRB.CreateUIToFP(PtrToInt,  Type::getDoubleTy(M->getContext()), "my_si_fp");

      Value *PtrToInt2 = IRB.CreatePtrToInt(Op2Addr, Int64Ty, "my_ptr_int");
      Value *FPVal2 = IRB.CreateUIToFP(PtrToInt2,  Type::getDoubleTy(M->getContext()), "my_si_fp");

      Value *FPValSize = IRB.CreateUIToFP(size,  Type::getDoubleTy(M->getContext()), "my_si_fp");

      BasicBlock *OldBB = I->getParent();
      BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
      createUpdateBlock3(FPVal, FPVal2, FPValSize, BufAddr, I, OldBB, Cont, F);
    }
  }
}

void FPSanitizer::handleMemCpy(CallInst *CI, BasicBlock *BB, Function *F,
                               std::string CallName) {
  
  Instruction *I = dyn_cast<Instruction>(CI);
  Instruction *Next = getNextInstruction(dyn_cast<Instruction>(CI), BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());

  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *debugInfoAvailable = ConstantInt::get(Int1Ty, debugInfoAvail);
  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);
  ConstantInt *colNumber = ConstantInt::get(Int32Ty, colNum);

  Value *Op1Addr = CI->getOperand(0);
  Value *Op2Addr = CI->getOperand(1);
  if (BitCastInst *BI = dyn_cast<BitCastInst>(Op1Addr)) {
    if (checkIfBitcastFromFP(BI)) {
      Value *Idx = BufIdxMap.at(F);
      FuncInit = M->getOrInsertFunction("fpsan_handle_memcpy", VoidTy, Int32Ty, Int64Ty,
                                        Int64Ty, Int64Ty);
      Value *ToAddr = readFromBuf(I, BB, F);
      Value *ToAddrInt = IRB.CreateFPToUI(ToAddr,  Type::getInt64Ty(M->getContext()), "my_si_fp");
      Value *FromAddr = readFromBuf(I, BB, F);
      Value *FromAddrInt = IRB.CreateFPToUI(FromAddr,  Type::getInt64Ty(M->getContext()), "my_si_fp");

      Value *Val = readFromBuf(I, BB, F);
      Value *ValInt = IRB.CreateFPToUI(Val, Type::getInt64Ty(M->getContext()), "my_si_fp");
      IRB.CreateCall(FuncInit, {Idx, ToAddrInt, FromAddrInt, ValInt});
    }
  }
  AllInstList.push_back(CI);
}

void FPSanitizer::handleStoreProducer(StoreInst *SI, BasicBlock *BB,
                                      Function *F) {
  Instruction *I = dyn_cast<Instruction>(SI);
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  LLVMContext &C = F->getContext();
  Value *OP = SI->getOperand(0);

  Value *Addr = SI->getPointerOperand();
  if(Addr->getName().startswith("my_"))
    return;

  Type *StoreTy = I->getOperand(0)->getType();
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());

  Type *VoidTy = Type::getVoidTy(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  ConstantInt *instId = GetInstId(F, I);

  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);
  bool BTFlag = false;
  Type *OpTy = OP->getType();
  // TODO: do we need to check for bitcast for store?
  int fType = 0;
  if (isFloatType(StoreTy) || StoreTy == MPtrTy || BTFlag) {
    Value *InsIndex;
    bool res = handleOperand(SI, OP, F, &InsIndex);
    if (res) { // handling registers
      Value *BufAddr = BufAddrMap.at(F);
      Value *PtrToInt = IRB.CreatePtrToInt(Addr, Int64Ty, "my_ptr_int");
      Value *FPVal = IRB.CreateUIToFP(PtrToInt,  Type::getDoubleTy(M->getContext()), "my_si_fp");
      BasicBlock *OldBB = I->getParent();
      BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
      createUpdateBlock(FPVal, BufAddr, I, OldBB, Cont, F);
    } else {
      if (isFloat(StoreTy)) {
        Value *BufAddr = BufAddrMap.at(F);
        Value *PtrToInt = IRB.CreatePtrToInt(Addr, Int64Ty, "my_ptr_int");
        Value *FPVal = IRB.CreateUIToFP(PtrToInt,  Type::getDoubleTy(M->getContext()), "my_si_fp");
        BasicBlock *OldBB = I->getParent();
        BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
        createUpdateBlock(FPVal, BufAddr, I, OldBB, Cont, F);
      } else if (isDouble(StoreTy)) {
        Value *BufAddr = BufAddrMap.at(F);
        Value *PtrToInt = IRB.CreatePtrToInt(Addr, Int64Ty, "my_ptr_int");
        Value *FPVal = IRB.CreateUIToFP(PtrToInt,  Type::getDoubleTy(M->getContext()), "my_si_fp");
        BasicBlock *OldBB = I->getParent();
        BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
        createUpdateBlock(FPVal, BufAddr, I, OldBB, Cont, F);
      }
    }
  }
}

void FPSanitizer::handleStore(StoreInst *SI, BasicBlock *BB, Function *F) {
  Value *SAddr = SI->getPointerOperand();
  if(SAddr->getName().startswith("my_"))
    return;
  if(SI->getName().startswith("my_store_buf"))
    return;
  Instruction *I = dyn_cast<Instruction>(SI);
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  LLVMContext &C = F->getContext();
  Value *OP = SI->getOperand(0);
  Value *Addr = SI->getPointerOperand();

  Type *StoreTy = I->getOperand(0)->getType();
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());

  Type *VoidTy = Type::getVoidTy(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);

  bool BTFlag = false;
  Type *OpTy = OP->getType();
  int fType = 0;

  if (isFloatType(StoreTy) || StoreTy == MPtrTy || BTFlag) {
    Value *Idx = BufIdxMap.at(F);
    Value *InsIndex;
    bool res = handleOperand(SI, OP, F, &InsIndex);
    if (res) { // handling registers
      SetRealTemp = M->getOrInsertFunction("fpsan_store_shadow", VoidTy,
                                           Int32Ty, Int64Ty, MPtrTy);
      Value *ToAddr = readFromBuf(I, BB, F);
      Value *ToAddrInt = IRB.CreateFPToUI(ToAddr,  Type::getInt64Ty(M->getContext()), "my_si_fp");
      Value *CALL = IRB.CreateCall(SetRealTemp, {Idx, ToAddrInt, InsIndex});
    } else {
      if (isFloat(StoreTy) || fType == 1) {
        SetRealTemp = M->getOrInsertFunction("fpsan_store_shadow_fconst", VoidTy, Int32Ty, Int64Ty,
                                   OpTy, instId->getType(), Int32Ty);
        Value *ToAddr = readFromBuf(I, BB, F);
        Value *ToAddrInt = IRB.CreateFPToUI(ToAddr,  Type::getInt64Ty(M->getContext()), "my_si_fp");
        IRB.CreateCall(SetRealTemp, {Idx, ToAddrInt, OP, instId, lineNumber});
      }
      // TODO: How to handle such cases in a better way?
      /*
         %119 = bitcast double* %118 to i64*, !dbg !835
          %120 = bitcast i64* %119 to i8*
        store i64 %117, i64* %119, align 8, !dbg !835, !tbaa !309
      */
      else if (isDouble(StoreTy) || fType == 2) {
        SetRealTemp = M->getOrInsertFunction("fpsan_store_shadow_dconst", VoidTy, Int32Ty,
                                   Int64Ty, OpTy, instId->getType(), Int32Ty);
        Value *ToAddr = readFromBuf(I, BB, F);
        Value *ToAddrInt = IRB.CreateFPToUI(ToAddr,  Type::getInt64Ty(M->getContext()), "my_si_fp");
        IRB.CreateCall(SetRealTemp, {Idx, ToAddrInt, OP, instId, lineNumber});
      }
    }
  }
  // delete store instruction
  AllInstList.push_back(SI);
}

void FPSanitizer::handleNewPhi(Function *F) {
  Module *M = F->getParent();
  Instruction *Next;
  long NumPhi = 0;
  BasicBlock *IBB, *BB;
  for (auto it = NewPhiMap.begin(); it != NewPhiMap.end(); ++it) {
    if (PHINode *PN = dyn_cast<PHINode>(it->first)) {
      PHINode *iPHI = dyn_cast<PHINode>(it->second);
      for (unsigned PI = 0, PE = PN->getNumIncomingValues(); PI != PE; ++PI) {
        IBB = PN->getIncomingBlock(PI);
        Value *IncValue = PN->getIncomingValue(PI);
        BB = PN->getParent();

        if (IncValue == PN)
          continue; // TODO
        Value *InsIndex;
        bool res = handleOperand(it->first, IncValue, F, &InsIndex);
        if (!res) {
          errs() << F->getName()
                 << ":handleNewPhi:Error !!! metadata not found for operand:"
                 << F->getName() << "\n";
          errs() << *IncValue;
          errs() << "In Inst:"
                 << "\n";
          errs() << *(it->first);
          exit(1);
        }
        iPHI->addIncoming(InsIndex, IBB);
      }
      AllInstList.push_back(PN);
    }
  }
}

void FPSanitizer::handlePhi(PHINode *PN, BasicBlock *BB, Function *F) {
  Module *M = F->getParent();
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *VoidTy = Type::getVoidTy(M->getContext());
  IRBuilder<> IRB(dyn_cast<Instruction>(dyn_cast<Instruction>(PN)));

  PHINode *iPHI = IRB.CreatePHI(MPtrTy, PN->getNumOperands(), "my_phi");
  // Wherever old phi node has been used, we need to replace it with
  // new phi node. That's why need to track it and keep it in RegIdMap
  MInsMap.insert(std::pair<Instruction *, Instruction *>(PN, iPHI));
  NewPhiMap.insert(std::pair<Instruction *, Instruction *>(dyn_cast<Instruction>(PN), iPHI));

  Instruction *Next;
  Next = getNextInstructionNotPhi(PN, BB);
  for (auto &I : *BB) {
    if (CallInst *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (Callee) {
        if (Callee->getName().startswith("fpsan_copy_phi")) {
          Next = getNextInstruction(&I, BB);
        }
      }
    }
  }

  IRBuilder<> IRBN(Next);
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());

  Value *BOGEP = GEPMap.at(PN);
  Value *Idx = BufIdxMap.at(F);

  AddFunArg = M->getOrInsertFunction("fpsan_copy_phi", VoidTy, Int32Ty, MPtrTy, MPtrTy);
  IRBN.CreateCall(AddFunArg, {Idx, iPHI, BOGEP});
}

void FPSanitizer::handleSelectProducer(SelectInst *SI, BasicBlock *BB,
    Function *F) {
  Instruction *I = dyn_cast<Instruction>(SI);
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *VoidTy = Type::getVoidTy(M->getContext());

  Value *BufAddr = BufAddrMap.at(F);
  Value *FPVal = IRB.CreateUIToFP(SI->getOperand(0),  Type::getDoubleTy(M->getContext()), "my_si_fp");
  BasicBlock *OldBB = I->getParent();
  BasicBlock *Cont = OldBB->splitBasicBlock(dyn_cast<Instruction>(FPVal)->getNextNode(), "split");
  createUpdateBlock(FPVal, BufAddr, I, OldBB, Cont, F);

  if(isa<FCmpInst>(dyn_cast<Instruction>(SI->getOperand(0)))){
    FCMPMapPush.insert(std::pair<Instruction *, Instruction *>(
          dyn_cast<Instruction>(SI->getOperand(0)), dyn_cast<Instruction>(SI->getOperand(0))));
  }
}

void FPSanitizer::handleSelect(SelectInst *SI, BasicBlock *BB, Function *F) {
  Instruction *I = dyn_cast<Instruction>(SI);
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  Value *InsIndex2, *InsIndex3;
  bool res1 = handleOperand(I, SI->getOperand(1), F, &InsIndex2);
  if (!res1) {
    errs() << F->getName()
           << " handleSelect: Error !!! metadata not found for op:"
           << "\n";
    errs()<<*(SI->getOperand(1))<<"\n";
    errs() << "In Inst:"
           << "\n";
    errs()<<*I<<"\n";
    exit(1);
  }
  bool res2 = handleOperand(I, SI->getOperand(2), F, &InsIndex3);
  if (!res2) {
    errs() << F->getName()
           << " handleSelect: Error !!! metadata not found for op:"
           << "\n";
    errs()<<*(SI->getOperand(2))<<"\n";
    errs() << "In Inst:"
           << "\n";
    errs()<<*I;
    exit(1);
  }

  FuncInit = M->getOrInsertFunction("fpsanx_pull_cond",
                                    Type::getInt1Ty(M->getContext()),
                                    Type::getInt32Ty(M->getContext()));
  Value *Idx = BufIdxMap.at(F);
 
  Value *Cond1 = readFromBuf(SI, BB, F);
  Value *Cond = IRB.CreateFPToUI(Cond1, Type::getInt1Ty(M->getContext()), "my_trunc");
  if(!isa<UndefValue>(SI->getOperand(0))){
    if(isa<FCmpInst>(dyn_cast<Instruction>(SI->getOperand(0)))){
      FCMPMap.insert(std::pair<Instruction *, Instruction *>(
            dyn_cast<Instruction>(SI->getOperand(0)), dyn_cast<Instruction>(Cond)));
    }
  }
  Value *NewOp2 = dyn_cast<Value>(InsIndex2);
  Value *NewOp3 = dyn_cast<Value>(InsIndex3);

  Type *FCIOpType = SI->getOperand(0)->getType();

  Value *Select = IRB.CreateSelect(Cond, NewOp2, NewOp3, "my_select");
  Instruction *NewIns = dyn_cast<Instruction>(Select);

  MInsMap.insert(std::pair<Instruction *, Instruction *>(SI, NewIns));
}

void FPSanitizer::handleReturn(ReturnInst *RI, BasicBlock *BB, Function *F) {
  Instruction *Ins = dyn_cast<Instruction>(RI);
  Module *M = F->getParent();

  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *VoidTy = Type::getVoidTy(M->getContext());

  Value *OpIdx;
  // Find first mpfr clear
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (CallInst *CI = dyn_cast<CallInst>(&I)) {
        Function *Callee = CI->getCalledFunction();
        if (Callee && Callee->getName().startswith("fpsanx_clear_mpfr")) {
          Ins = &I;
          break;
        }
      }
    }
  }

  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IRBuilder<> IRB(Ins);
 
  if (std::find(SliceList.begin(), SliceList.end(), OrigFuncMap.at(F)) == SliceList.end()) {
    if (RI->getNumOperands() != 0) {
      Value *OP = RI->getOperand(0);
      if (isFloatType(OP->getType())) {
        bool res = handleOperand(dyn_cast<Instruction>(RI), OP, F, &OpIdx);
        if (!res) {
          errs() << "\nhandleReturn: Error !!! metadata not found for op in:"
            << F->getName() << "\n";
          errs() << "In Inst:"
            << "\n";
          errs() << *OP << "\n";
          errs() << *F << "\n";
          exit(1);
        }
        LLVMContext &C = F->getContext();
        Value *Idx = BufIdxMap.at(F);
        AddFunArg = M->getOrInsertFunction("fpsan_copy_return", VoidTy, Int32Ty,
            MPtrTy, MPtrTy);
        IRB.CreateCall(AddFunArg, {Idx, OpIdx, F->getArg(F->arg_size() - 1)});
      }
    }
  }
  else{
    if (RI->getNumOperands() != 0) {
      Value *OP = RI->getOperand(0);
      if (isFloatType(OP->getType())) {
        bool res = handleOperand(dyn_cast<Instruction>(RI), OP, F, &OpIdx);
        if (!res) {
          errs() << "\nhandleReturn: Error !!! metadata not found for op in:"
            << F->getName() << "\n";
          errs() << "In Inst:"
            << "\n";
          errs() << *OP << "\n";
          errs() << *F << "\n";
          exit(1);
        }
        LLVMContext &C = F->getContext();
        Value *Idx = BufIdxMap.at(F);
        AddFunArg = M->getOrInsertFunction("fpsan_check_error", VoidTy, Int32Ty,
            MPtrTy);
        IRB.CreateCall(AddFunArg, {Idx, OpIdx});
      }
    }
  }
  FuncInit = M->getOrInsertFunction("fpsan_func_exit", VoidTy, Int32Ty);
  long TotalArgs = FuncTotalArg.at(F);
  if (BufIdxMap.count(F) > 0) {
    Value *Idx = BufIdxMap.at(F);
    IRB.CreateCall(FuncInit, {Idx});
  }
}

//This function create a load for my_slice_flag_load once at the start of the function,
//to avoid load in each u_cmp block
void FPSanitizer::handleSliceFlag(Function *F){
  if (F->getName().startswith("start_slice") ||
      F->getName().startswith("end_slice")) {
    return;
  }
  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;

  for (auto &I : BB) {
    if (CallInst *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (Callee && (Callee->getName() == "fpsanx_get_buf_idx")) {
        First = &I;
      }
    }
  }
  IRBuilder<> IRB(First->getNextNode());
  Value *SLiceF = IRB.CreateLoad(IRB.getInt8Ty(), SliceFlag, "my_slice_flag_load");
  SliceFlagMap.insert(std::pair<Function *, Instruction *>(F, dyn_cast<Instruction>(SLiceF)));
  Value *Counter = IRB.CreateLoad(IRB.getInt64Ty(), QIdx, "my_load_idx");
  QIdxMap.insert(std::pair<BasicBlock *, Instruction *>(&BB, dyn_cast<Instruction>(Counter)));
}

void FPSanitizer::handleStoreIdxProducerCallInst(Instruction *Ins, BasicBlock *BB, Function *F) {
  if (F->getName().startswith("start_slice") ||
      F->getName().startswith("end_slice") ||
      F->getName().startswith("fpsan")) {
    return;
  }
  Module *M = F->getParent();

  IRBuilder<> IRB(Ins);
  IRBuilder<> IRBAfter(Ins->getNextNode());
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *VoidTy = Type::getVoidTy(M->getContext());
  BasicBlock *Pred;
  Value *Counter;

  if(QIdxMap.count(BB) != 0){
    Counter = QIdxMap.at(BB);
  }
  else{
    for (pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; ++PI){
      Pred = *PI;
      if(QIdxMap.count(Pred) != 0){
        Counter = QIdxMap.at(Pred);
      }
    }
  }

  bool flag = false;
  CallInst *CI = dyn_cast<CallInst>(Ins);
  Function *Callee = CI->getCalledFunction();
  size_t NumOperands = CI->getNumArgOperands();
  if (std::find(SliceList.begin(), SliceList.end(), Callee) != SliceList.end()) {
    for (int i = 0; i < NumOperands; i++) {
      Value *Op = CI->getArgOperand(i);
      if(isFloatType(Op->getType())){ 
        flag = true;
      }
    }
  }
  // We want to push arguments only we called function is a slice, otherwise arguments will be propagated
  // via temporary entry. 
  if (std::find(SliceList.begin(), SliceList.end(), Callee) != SliceList.end()) {
    Value *BufAddr = BufAddrMap.at(F);
    Counter = createUpdateBlockArg(BufAddr, Ins, F, Counter);
    AllBranchList.push_back(dyn_cast<Instruction>(CI));
  }

  IRB.CreateStore(Counter, QIdx, "my_store_idx");
  Value *UpdatedCounter = IRBAfter.CreateLoad(IRB.getInt64Ty(), QIdx, "my_load_idx");

  std::map<BasicBlock*, Instruction*>::iterator it = QIdxMap.find(BB); 
  if(it !=  QIdxMap.end()){
    it->second = dyn_cast<Instruction>(UpdatedCounter);
  }
  else{
    QIdxMap.insert(std::pair<BasicBlock *, Instruction *>(BB, dyn_cast<Instruction>(UpdatedCounter)));
  }
}

void FPSanitizer::handleStoreIdxProducer(Instruction *Ins, BasicBlock *BB, Function *F) {
  if (F->getName().startswith("start_slice") ||
      F->getName().startswith("end_slice") ||
      F->getName().startswith("fpsan")) {
    return;
  }
  Module *M = F->getParent();

  for (auto &I : *BB) {
    if (CallInst *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (Callee) {
        if (Callee->getName().startswith("fpsan_slice_end")) {
          Ins = I.getPrevNode();
        }
      }
    }
  }
  IRBuilder<> IRB(Ins);
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *VoidTy = Type::getVoidTy(M->getContext());
  BasicBlock *Pred;
  Value *Counter;
  if(QIdxMap.count(BB) != 0){
    Counter = QIdxMap.at(BB);
  }
  else{
    for (pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; ++PI){
      Pred = *PI;
      if(QIdxMap.count(Pred) != 0){
        Counter = QIdxMap.at(Pred);
      }
    }
  }
  IRB.CreateStore(Counter, QIdx, "my_store_idx");
}

void FPSanitizer::createUpdateBlock2(Value *VAddr, Value *val, Value *Addr, Instruction *I, BasicBlock *OldB, BasicBlock *ContB, Function *F) {
  if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
    if (std::find(AllBranchList.begin(), AllBranchList.end(), I) != AllBranchList.end()) {
      return;
    }
  }
  else if (SwitchInst *BI = dyn_cast<SwitchInst>(I)) {
    if (std::find(AllBranchList.begin(), AllBranchList.end(), I) != AllBranchList.end()) {
      return;
    }
  }
  Module *M = F->getParent();
  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int8Ty = Type::getInt8Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *PtrDoubleTy = PointerType::getUnqual(Type::getDoubleTy(M->getContext()));

  BasicBlock *NewBB = BasicBlock::Create(M->getContext(), "u_cmp", F);

  //remove old branch instruction and update with new branch instruction
  Instruction *BrInst = OldB->getTerminator();
  BrInst->eraseFromParent();

  BranchInst *BJCmp = BranchInst::Create(NewBB, OldB);
  AllBranchList.push_back(dyn_cast<Instruction>(BJCmp));
  IRBuilder<> IRBE(NewBB);

  BasicBlock *UpdateB = BasicBlock::Create(M->getContext(), "update", F);

  Value *SLiceF = SliceFlagMap.at(F);

//  FuncInit = M->getOrInsertFunction("fpsanx_print_slice_flag", VoidTy, SLiceF->getType());
//  IRBE.CreateCall(FuncInit, {SLiceF});
  Value* Cond = IRBE.CreateICmp(ICmpInst::ICMP_EQ, SLiceF, ConstantInt::get(Int8Ty, 1));
  BranchInst *BInst = BranchInst::Create(/*ifTrue*/UpdateB, /*ifFalse*/ContB, Cond, NewBB);
  AllBranchList.push_back(dyn_cast<Instruction>(BInst));
#if 1
  IRBuilder<> IRB(UpdateB);

  //store addr
  Value *Counter = IRB.CreateLoad(IRB.getInt64Ty(), QIdx, "my_load_idx");
  Value *GEP = IRB.CreateGEP(Type::getDoubleTy(M->getContext()), Addr, Counter, "my_gep_buf");
  Value *ValStore = IRB.CreateStore(VAddr, GEP, "my_store_buf");
  if(DEBUG_P){
    FuncInit = M->getOrInsertFunction("fpsanx_push_print", VoidTy, VAddr->getType(), Counter->getType());
    IRB.CreateCall(FuncInit, {VAddr, Counter});
  }
  Counter = IRB.CreateAdd(Counter, ConstantInt::get(Int64Ty, 1), "my_incr_idx");
  //store val
  GEP = IRB.CreateGEP(Type::getDoubleTy(M->getContext()), Addr, Counter, "my_gep_buf");
  ValStore = IRB.CreateStore(val, GEP, "my_store_buf");
  if(DEBUG_P){
    FuncInit = M->getOrInsertFunction("fpsanx_push_print", VoidTy, val->getType(), Counter->getType());
    IRB.CreateCall(FuncInit, {val, Counter});
  }
  Counter = IRB.CreateAdd(Counter, ConstantInt::get(Int64Ty, 1), "my_incr_idx");
  IRB.CreateStore(Counter, QIdx, "my_store_idx");
  BranchInst *BI = BranchInst::Create(ContB, UpdateB);
  AllBranchList.push_back(dyn_cast<Instruction>(BI));
#endif
}

void FPSanitizer::createUpdateBlock3(Value *VAddr1, Value *VAddr2, Value *Val, 
                                     Value *Addr, Instruction *I, BasicBlock *OldB, 
                                     BasicBlock *ContB, Function *F) {
  if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
    if (std::find(AllBranchList.begin(), AllBranchList.end(), I) != AllBranchList.end()) {
      return;
    }
  }
  else if (SwitchInst *BI = dyn_cast<SwitchInst>(I)) {
    if (std::find(AllBranchList.begin(), AllBranchList.end(), I) != AllBranchList.end()) {
      return;
    }
  }
  Module *M = F->getParent();
  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int8Ty = Type::getInt8Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *PtrDoubleTy = PointerType::getUnqual(Type::getDoubleTy(M->getContext()));

  BasicBlock *NewBB = BasicBlock::Create(M->getContext(), "u_cmp", F);

  //remove old branch instruction and update with new branch instruction
  Instruction *BrInst = OldB->getTerminator();
  BrInst->eraseFromParent();

  BranchInst *BJCmp = BranchInst::Create(NewBB, OldB);
  AllBranchList.push_back(dyn_cast<Instruction>(BJCmp));
  IRBuilder<> IRBE(NewBB);

  BasicBlock *UpdateB = BasicBlock::Create(M->getContext(), "update", F);

  Value *SLiceF = SliceFlagMap.at(F);

//  FuncInit = M->getOrInsertFunction("fpsanx_print_slice_flag", VoidTy, SLiceF->getType());
//  IRBE.CreateCall(FuncInit, {SLiceF});
  Value* Cond = IRBE.CreateICmp(ICmpInst::ICMP_EQ, SLiceF, ConstantInt::get(Int8Ty, 1));
  BranchInst *BInst = BranchInst::Create(/*ifTrue*/UpdateB, /*ifFalse*/ContB, Cond, NewBB);
  AllBranchList.push_back(dyn_cast<Instruction>(BInst));
#if 1
  IRBuilder<> IRB(UpdateB);

  //store addr1
  Value *Counter = IRB.CreateLoad(IRB.getInt64Ty(), QIdx, "my_load_idx");
  Value *GEP = IRB.CreateGEP(Type::getDoubleTy(M->getContext()), Addr, Counter, "my_gep_buf");
  Value *ValStore = IRB.CreateStore(VAddr1, GEP, "my_store_buf");
  if(DEBUG_P){
    FuncInit = M->getOrInsertFunction("fpsanx_push_print", VoidTy, VAddr1->getType(), Counter->getType());
    IRB.CreateCall(FuncInit, {VAddr1, Counter});
  }
  Counter = IRB.CreateAdd(Counter, ConstantInt::get(Int64Ty, 1), "my_incr_idx");

  //store addr2
  GEP = IRB.CreateGEP(Type::getDoubleTy(M->getContext()), Addr, Counter, "my_gep_buf");
  ValStore = IRB.CreateStore(VAddr2, GEP, "my_store_buf");
  if(DEBUG_P){
    FuncInit = M->getOrInsertFunction("fpsanx_push_print", VoidTy, VAddr2->getType(), Counter->getType());
    IRB.CreateCall(FuncInit, {VAddr2, Counter});
  }
  Counter = IRB.CreateAdd(Counter, ConstantInt::get(Int64Ty, 1), "my_incr_idx");

  //store val
  GEP = IRB.CreateGEP(Type::getDoubleTy(M->getContext()), Addr, Counter, "my_gep_buf");
  ValStore = IRB.CreateStore(Val, GEP, "my_store_buf");
  if(DEBUG_P){
    FuncInit = M->getOrInsertFunction("fpsanx_push_print", VoidTy, Val->getType(), Counter->getType());
    IRB.CreateCall(FuncInit, {Val, Counter});
  }
  Counter = IRB.CreateAdd(Counter, ConstantInt::get(Int64Ty, 1), "my_incr_idx");
  IRB.CreateStore(Counter, QIdx, "my_store_idx");
  BranchInst *BI = BranchInst::Create(ContB, UpdateB);
  AllBranchList.push_back(dyn_cast<Instruction>(BI));
#endif
}

void FPSanitizer::addIncomingPhi(BasicBlock *BB){
  Module *M = BB->getParent()->getParent();
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  SmallVector<BasicBlock *, 8> PredList;
  for (auto &I : *BB){
    if (PHINode *PN = dyn_cast<PHINode>(&I)) {
      if(QIdxMap.count(BB) > 0){
        Instruction *iPHIns = QIdxMap.at(BB);
        if(PHINode *iPHI = dyn_cast<PHINode>(iPHIns)){
          if(iPHI == PN){
            if(iPHI->getNumIncomingValues() == 0){
              for (pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; ++PI){
                BasicBlock *Pred = *PI;
                Value *Counter = QIdxMap.at(Pred);
                iPHI->addIncoming(Counter, Pred);
              }
            }
          }
        }
        if(LoadInst *LI = dyn_cast<LoadInst>(iPHIns)){
          if(PN->getNumIncomingValues() == 0){
            for (pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; ++PI){
              BasicBlock *Pred = *PI;
              Value *Counter = QIdxMap.at(Pred);
              PN->addIncoming(Counter, Pred);
            }
          }
        }
      }
    }
  }
}

void FPSanitizer::createPhiCounter(BasicBlock *BB){
  Module *M = BB->getParent()->getParent();
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  if(BB->getName().startswith("split")){
    BasicBlock::iterator BBit = BB->begin();
    Instruction *First = &*BBit;
    IRBuilder<> IRBO(First);
    PHINode *iPHI = IRBO.CreatePHI(Int64Ty, 2, "my_phi");
    QIdxMap.insert(std::pair<BasicBlock *, Instruction *>(BB, dyn_cast<Instruction>(iPHI)));
  }
  else{
    SmallVector<BasicBlock *, 8> PredList;
    for (pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; ++PI){
      BasicBlock *Pred = *PI;
      PredList.push_back(Pred);
    }
    if(PredList.size() > 1){
      if(QIdxMap.count(BB) == 0){
        BasicBlock::iterator BBit = BB->begin();
        Instruction *First = &*BBit;
        IRBuilder<> IRBO(First);
        PHINode *iPHI = IRBO.CreatePHI(Int64Ty, PredList.size(), "my_phi");
        QIdxMap.insert(std::pair<BasicBlock *, Instruction *>(BB, dyn_cast<Instruction>(iPHI)));
      }
    }
  }
}

void FPSanitizer::updateUpdateBlock(BasicBlock *BB){
  Module *M = BB->getParent()->getParent();
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  if(!BB->getName().startswith("update")){
    return;
  }
  BasicBlock::iterator BBit = BB->begin();
  Instruction *First = &*BBit;
  BasicBlock *Pred, *PPred;
  Value *Counter;
  for (pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; ++PI){
    Pred = *PI; //u_cmp
    for (pred_iterator PPI = pred_begin(Pred), EE = pred_end(Pred); PPI != EE; ++PPI){
      PPred = *PPI;
      if(QIdxMap.count(PPred) != 0){
        Counter = QIdxMap.at(PPred);
        QIdxMap.insert(std::pair<BasicBlock *, Instruction *>(Pred, dyn_cast<Instruction>(Counter)));
      }
    }
  }
  First->replaceAllUsesWith(Counter);
  //Delete Load
  First->eraseFromParent();
  Instruction *BrInst = BB->getTerminator();
  Instruction *StoreInst = BrInst->getPrevNode();
  Value *UpdateIdx = StoreInst->getOperand(0);
  QIdxMap.insert(std::pair<BasicBlock *, Instruction *>(BB, dyn_cast<Instruction>(UpdateIdx)));
  //Delete Store
  StoreInst->eraseFromParent();
}

void FPSanitizer::propagatePhiCounter(BasicBlock *BB){
  Module *M = BB->getParent()->getParent();
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  SmallVector<BasicBlock *, 8> PredList;
  for (pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; ++PI){
    BasicBlock *Pred = *PI;
    if(Pred->getName().startswith("split")){
      PredList.push_back(Pred);
    }
  }

//This propagates index to all blocks which have 1 split block as pred
  if(PredList.size() == 1 && pred_size(BB) == 1){
    BasicBlock::iterator BBit = BB->begin();
    Instruction *First = &*BBit;
    IRBuilder<> IRBO(First);
    if(QIdxMap.count(BB) > 0){
      Instruction *iPHI = QIdxMap.at(BB);
      QIdxMap.insert(std::pair<BasicBlock *, Instruction *>(BB, iPHI));
    }
    else{
      Instruction *iPHI = QIdxMap.at(PredList[0]);
      QIdxMap.insert(std::pair<BasicBlock *, Instruction *>(BB, iPHI));
    }
  }
}

Value* FPSanitizer::createUpdateBlockArg(Value *Addr, Instruction *I, 
                          Function *F, Value *Counter) {
  if (std::find(AllBranchList.begin(), AllBranchList.end(), I) != AllBranchList.end()) {
    return nullptr;
  }

  Module *M = F->getParent();
  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int8Ty = Type::getInt8Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *PtrDoubleTy = PointerType::getUnqual(Type::getDoubleTy(M->getContext()));

  CallInst *CI = dyn_cast<CallInst>(I);
  size_t NumOperands = CI->getNumArgOperands();
  Value *BufAddr = BufAddrMap.at(F);

  //Remove old branch instruction and update with new branch instruction

#if 1
  IRBuilder<> IRB(I);

  for (int i = 0; i < NumOperands; i++) {
    Value *Op = CI->getArgOperand(i);
    if(isFloat(Op->getType())){
      Op = IRB.CreateFPExt(Op, Type::getDoubleTy(M->getContext()));
    }
    if(isFloatType(Op->getType())){ 
      Value *GEP = IRB.CreateGEP(Type::getDoubleTy(M->getContext()), Addr, Counter, "my_gep_buf");
      Value *ValStore = IRB.CreateStore(Op, GEP, "my_store_buf");
      if(DEBUG_P){
        FuncInit = M->getOrInsertFunction("fpsanx_push_print", VoidTy, Op->getType(), Counter->getType());
        IRB.CreateCall(FuncInit, {Op, Counter});
      }
      Counter = IRB.CreateAdd(Counter, ConstantInt::get(Int64Ty, 1), "my_incr_idx");
    }
  }
  return Counter;
#endif
}

Value* FPSanitizer::readFromBuf(Instruction *I, BasicBlock *BB, Function *F) {
  Module *M = F->getParent();
  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int8Ty = Type::getInt8Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *PtrDoubleTy = PointerType::getUnqual(Type::getDoubleTy(M->getContext()));

  Value *Addr = CBufAddrMap.at(F);
  IRBuilder<> IRB(I);

  Value *Counter = IRB.CreateLoad(IRB.getInt64Ty(), CQIdx, "my_load_idx_c");
  Value *GEP = IRB.CreateGEP(Type::getDoubleTy(M->getContext()), Addr, Counter, "my_gep_buf_c");
  Value *Val = IRB.CreateLoad(Type::getDoubleTy(M->getContext()), GEP, "my_load_val_c");
  if(DEBUG_P){
    FunctionCallee FuncPrint = M->getOrInsertFunction("pull_print", VoidTy, Val->getType(), Counter->getType());
    IRB.CreateCall(FuncPrint, {Val, Counter});
  }
  Value *Add = IRB.CreateAdd(Counter, ConstantInt::get(Int64Ty, 1), "my_incr_idx_c");
  IRB.CreateStore(Add, CQIdx, "my_store_idx_c");
  return Val;
}

void FPSanitizer::createUpdateBlock(Value *val, Value *Addr, Instruction *I, BasicBlock *OldB, BasicBlock *ContB, Function *F) {
  if (BranchInst *BI = dyn_cast<BranchInst>(I)) {
    if (std::find(AllBranchList.begin(), AllBranchList.end(), I) != AllBranchList.end()) {
      return;
    }
  }
  else if (SwitchInst *BI = dyn_cast<SwitchInst>(I)) {
    if (std::find(AllBranchList.begin(), AllBranchList.end(), I) != AllBranchList.end()) {
      return;
    }
  }
  Module *M = F->getParent();
  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int8Ty = Type::getInt8Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  Type *PtrDoubleTy = PointerType::getUnqual(Type::getDoubleTy(M->getContext()));

  BasicBlock *NewBB = BasicBlock::Create(M->getContext(), "u_cmp", F);

  //remove old branch instruction and update with new branch instruction
  Instruction *BrInst = OldB->getTerminator();
  BrInst->eraseFromParent();

  BranchInst *BJCmp = BranchInst::Create(NewBB, OldB);
  AllBranchList.push_back(dyn_cast<Instruction>(BJCmp));
  IRBuilder<> IRBE(NewBB);

  BasicBlock *UpdateB = BasicBlock::Create(M->getContext(), "update", F);

  Value *SLiceF = SliceFlagMap.at(F);

//  FuncInit = M->getOrInsertFunction("fpsanx_print_slice_flag", VoidTy, SLiceF->getType());
//  IRBE.CreateCall(FuncInit, {SLiceF});
  Value* Cond = IRBE.CreateICmp(ICmpInst::ICMP_EQ, SLiceF, ConstantInt::get(Int8Ty, 1));
  BranchInst *BInst = BranchInst::Create(/*ifTrue*/UpdateB, /*ifFalse*/ContB, Cond, NewBB);
  AllBranchList.push_back(dyn_cast<Instruction>(BInst));
#if 1
  IRBuilder<> IRB(UpdateB);

  Value *Counter = IRB.CreateLoad(IRB.getInt64Ty(), QIdx, "my_load_idx");
  Value *GEP = IRB.CreateGEP(Type::getDoubleTy(M->getContext()), Addr, Counter, "my_gep_buf");
  Value *ValStore = IRB.CreateStore(val, GEP, "my_store_buf");
  if(DEBUG_P){
    FuncInit = M->getOrInsertFunction("fpsanx_push_print", VoidTy, val->getType(), Counter->getType());
    IRB.CreateCall(FuncInit, {val, Counter});
  }
  Value *Add = IRB.CreateAdd(Counter, ConstantInt::get(Int64Ty, 1), "my_incr_idx");
  IRB.CreateStore(Add, QIdx, "my_store_idx");
  BranchInst *BI = BranchInst::Create(ContB, UpdateB);
  AllBranchList.push_back(dyn_cast<Instruction>(BI));
#endif
}

void FPSanitizer::handleFNeg(UnaryOperator *UO, BasicBlock *BB, Function *F) {
  Instruction *I = dyn_cast<Instruction>(UO);
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  Value *InsIndex1;
  bool res1 = handleOperand(I, UO->getOperand(0), F, &InsIndex1);
  if (!res1) {
    errs() << *F << "\n";
    errs() << "handleFNeg: Error !!! metadata not found for op:"
           << "\n";
    errs() << *UO->getOperand(0);
    errs() << "In Inst:"
           << "\n";
    errs() << *I;
    exit(1);
  }

  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);

  Value *BOGEP = GEPMap.at(I);

  std::string opName(I->getOpcodeName());
  Value *Idx = BufIdxMap.at(F);

  ComputeReal = M->getOrInsertFunction("fpsan_mpfr_fneg", VoidTy, Int32Ty,
                                       MPtrTy, MPtrTy);

  IRB.CreateCall(ComputeReal, {Idx, InsIndex1, BOGEP});
  MInsMap.insert(
      std::pair<Instruction *, Instruction *>(I, dyn_cast<Instruction>(BOGEP)));
}

void FPSanitizer::handleBinOp(BinaryOperator *BO, BasicBlock *BB, Function *F) {
  Instruction *I = dyn_cast<Instruction>(BO);
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();

  Value *InsIndex1, *InsIndex2;
  bool res1 = handleOperand(I, BO->getOperand(0), F, &InsIndex1);
  if (!res1) {
    errs() << "handleBinOp: Error !!! metadata not found for op:"<<F->getName()
      << "\n";
    errs() << *I;
    errs() << *F;
    errs() << *BO->getOperand(0);
    errs() << "In Inst:"
      << "\n";
    exit(1);
  }

  bool res2 = handleOperand(I, BO->getOperand(1), F, &InsIndex2);
  if (!res2) {
    errs() << "handleBinOp: Error !!! metadata not found for op:"<<F->getName()
      << "\n";
    errs() << *I;
    errs() << *F;
    errs() << *BO->getOperand(1);
    errs() << "In Inst:"
      << "\n";
    exit(1);
  }

  Type *BOType = BO->getOperand(0)->getType();
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *VoidTy = Type::getVoidTy(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }
  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);
  Value *BOGEP = GEPMap.at(I);

  std::string opName(I->getOpcodeName());
  Value *Idx = BufIdxMap.at(F);
  if (isFloat(BO->getType())) {
    ComputeReal = M->getOrInsertFunction("fpsan_mpfr_" + opName + "_f", VoidTy, Int32Ty,
        MPtrTy, MPtrTy, MPtrTy, instId->getType(), Int32Ty);
  } else if (isDouble(BO->getType())) {
    ComputeReal = M->getOrInsertFunction("fpsan_mpfr_" + opName, VoidTy, Int32Ty, MPtrTy,
        MPtrTy, MPtrTy, instId->getType(), Int32Ty);
  }
  IRB.CreateCall(ComputeReal, {Idx, InsIndex1, InsIndex2, BOGEP, instId, lineNumber});
  MInsMap.insert(std::pair<Instruction *, Instruction *>(I, dyn_cast<Instruction>(BOGEP)));
}

void FPSanitizer::handleFcmpProducer(FCmpInst *FCI, BasicBlock *BB, Function *F) {
  Instruction *I = dyn_cast<Instruction>(FCI);
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);
  Instruction *Computed;
  Module *M = F->getParent();
  if (FCMPMapPush.count(I) > 0) {
    return;    
  } 
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  Type *VoidTy = Type::getVoidTy(M->getContext());
  Value *BufAddr = BufAddrMap.at(F);
  Value *FPVal = IRB.CreateUIToFP(FCI,  Type::getDoubleTy(M->getContext()), "my_si_fp");
  BasicBlock *OldBB = I->getParent();
  BasicBlock *Cont = OldBB->splitBasicBlock(dyn_cast<Instruction>(FPVal)->getNextNode(), "split");
  createUpdateBlock(FPVal, BufAddr, I, OldBB, Cont, F);
}

void FPSanitizer::handleFcmp(FCmpInst *FCI, BasicBlock *BB, Function *F) {
  Instruction *I = dyn_cast<Instruction>(FCI);
  Instruction *Computed;
  Instruction *Next = getNextInstruction(FCI, BB);
  IRBuilder<> IRB(Next);
  Module *M = F->getParent();
  if (FCMPMap.count(I) > 0) {
    Computed = FCMPMap.at(I);
  } else {
    FuncInit = M->getOrInsertFunction("fpsanx_pull_cond",
                                    Type::getInt1Ty(M->getContext()),
                                    Type::getInt32Ty(M->getContext()));
    Value *Idx = BufIdxMap.at(F);
    Value *Cond1 = readFromBuf(Next, BB, F);
    Value *Cond = IRB.CreateFPToUI(Cond1, Type::getInt1Ty(M->getContext()), "my_trunc");
    Computed = dyn_cast<Instruction>(Cond);
  }

  IRBuilder<> IRBB(Computed->getNextNode());
  Value *InsIndex1, *InsIndex2;
  // We don't want to instrument fpsanx_pull_value
  if (CallInst *CI = dyn_cast<CallInst>(FCI->getOperand(0))) {
    Function *Callee = CI->getCalledFunction();
    if (Callee && Callee->getName().startswith("fpsanx_pull_value")) {
      return;
    }
  }
  bool res1 = handleOperand(I, FCI->getOperand(0), F, &InsIndex1);

  if (!res1) {
    errs() << "handleFcmp: Error !!! metadata not found for op0:"
           << F->getName() << "\n";
    errs() << *(FCI->getOperand(0));
    errs() << "In Inst:"
           << "\n";
    errs() << *FCI;
    exit(1);
  }
  bool res2 = handleOperand(I, FCI->getOperand(1), F, &InsIndex2);
  if (!res2) {
    errs() << "handleFcmp: Error !!! metadata not found for op1:"
           << F->getName() << "\n";
    errs() << *(FCI->getOperand(1));
    errs() << "In Inst:"
           << "\n";
    errs() << *I;
    exit(1);
  }
  Type *FCIOpType = FCI->getOperand(0)->getType();
  Type *Int64Ty = Type::getInt64Ty(M->getContext());
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());

  ConstantInt *instId = GetInstId(F, I);
  const DebugLoc &instDebugLoc = I->getDebugLoc();
  bool debugInfoAvail = false;
  unsigned int lineNum = 0;
  unsigned int colNum = 0;
  if (instDebugLoc) {
    debugInfoAvail = true;
    lineNum = instDebugLoc.getLine();
    colNum = instDebugLoc.getCol();
    if (lineNum == 0 && colNum == 0)
      debugInfoAvail = false;
  }

  ConstantInt *lineNumber = ConstantInt::get(Int32Ty, lineNum);

  Constant *OpCode =
      ConstantInt::get(Type::getInt64Ty(M->getContext()), FCI->getPredicate());

  FunctionCallee FCMPCallOp1, FCMPCallOp2;
  Value *Idx = BufIdxMap.at(F);
  if (isFloat(FCIOpType) || FCIOpType == MPtrTy) {
    CheckBranch =
        M->getOrInsertFunction("fpsan_check_branch_f", Int1Ty, Int32Ty, MPtrTy,
                               MPtrTy, Int64Ty, Int1Ty, instId->getType(), Int32Ty);
  } else if (isDouble(FCIOpType) || FCIOpType == MPtrTy) {
    CheckBranch =
        M->getOrInsertFunction("fpsan_check_branch_d", Int1Ty, Int32Ty, MPtrTy,
                               MPtrTy, Int64Ty, Int1Ty, instId->getType(), Int32Ty);
  }
  IRBB.CreateCall(CheckBranch, {Idx, InsIndex1, InsIndex2, OpCode, Computed, instId, lineNumber});

  AllInstList.push_back(FCI);
}

bool FPSanitizer::getArrayFloatType(ArrayType *AT) {
  if (isFloatType(AT->getElementType())) {
    if (isFloat(AT->getElementType())) {
      return 1;
    }
    else{
      return 2;
    }
  } else if (AT->getElementType()->getTypeID() == Type::ArrayTyID) {
    return getArrayFloatType(cast<ArrayType>(AT->getElementType()));
  }
  return 0;
}

bool FPSanitizer::isArrayFloat(ArrayType *AT) {
  if (isFloatType(AT->getElementType())) {
    return true;
  } else if (AT->getElementType()->getTypeID() == Type::ArrayTyID) {
    return isArrayFloat(cast<ArrayType>(AT->getElementType()));
  }
  return false;
}

bool FPSanitizer::isPointerFloat(PointerType *AT) {
  if (isFloatType(AT->getElementType())) {
    return true;
  } else if (AT->getElementType()->getTypeID() == Type::PointerTyID) {
    return isPointerFloat(cast<PointerType>(AT->getElementType()));
  } else if (AT->getElementType()->getTypeID() == Type::ArrayTyID) {
    return isArrayFloat(cast<ArrayType>(AT->getElementType()));
  }
  return false;
}

bool FPSanitizer::getPointerFloatType(PointerType *AT) {
  if (isFloatType(AT->getElementType())) {
    if (isFloat(AT->getElementType())) {
      return 1;
    }
    else{
      return 2;
    }
  } else if (AT->getElementType()->getTypeID() == Type::PointerTyID) {
    return getPointerFloatType(cast<PointerType>(AT->getElementType()));
  } else if (AT->getElementType()->getTypeID() == Type::ArrayTyID) {
    return getArrayFloatType(cast<ArrayType>(AT->getElementType()));
  }
  return 0;
}

//return 1 fiir float and 2 for double
int FPSanitizer::getBitcastFromFPType(BitCastInst *BI) {
  int BTFlag = 0;
  Type *BITy = BI->getOperand(0)->getType();
  // check if load operand is bitcast and bitcast operand is float type
  // check if load operand is bitcast and bitcast operand is struct type.
  // check if struct has any member of float type
  if (isFloatType(BITy)) {
    if(isFloat(BITy)){
      BTFlag = 1;
    }
    else{
      BTFlag = 2;
    }
  } else if (BITy->getPointerElementType()->getTypeID() == Type::StructTyID) {
    StructType *STyL = cast<StructType>(BITy->getPointerElementType());
    int num = STyL->getNumElements();
    for (int i = 0; i < num; i++) {
      if (isFloatType(STyL->getElementType(i))){
        if (isFloat(STyL->getElementType(i))){
          BTFlag = 1;
        }
        else{
          BTFlag = 2;
        }
      }
    }
  } else if (BITy->getPointerElementType()->getTypeID() == Type::ArrayTyID) {
    ArrayType *STyL = cast<ArrayType>(BITy->getPointerElementType());
    if (isArrayFloat(STyL)) {
      BTFlag = getArrayFloatType(STyL);
    }
  } else if (BITy->getPointerElementType()->getTypeID() == Type::PointerTyID) {
    PointerType *STyL = cast<PointerType>(BITy->getPointerElementType());
    if (isPointerFloat(STyL)) {
      BTFlag = getPointerFloatType(STyL);
    }
  } else if (isFloatType(BITy->getPointerElementType())) {
    if (isFloat(BITy->getPointerElementType())){ 
      BTFlag = 1;
    }
    else{
      BTFlag = 2;
    }
  }
  return BTFlag;
}

bool FPSanitizer::checkIfBitcastFromFP(BitCastInst *BI) {
  bool BTFlag = false;
  Type *BITy = BI->getOperand(0)->getType();
  // check if load operand is bitcast and bitcast operand is float type
  // check if load operand is bitcast and bitcast operand is struct type.
  // check if struct has any member of float type
  if (isFloatType(BITy)) {
    BTFlag = true;
  } else if (BITy->getPointerElementType()->getTypeID() == Type::StructTyID) {
    StructType *STyL = cast<StructType>(BITy->getPointerElementType());
    int num = STyL->getNumElements();
    for (int i = 0; i < num; i++) {
      if (isFloatType(STyL->getElementType(i)))
        BTFlag = true;
    }
  } else if (BITy->getPointerElementType()->getTypeID() == Type::ArrayTyID) {
    ArrayType *STyL = cast<ArrayType>(BITy->getPointerElementType());
    if (isArrayFloat(STyL)) {
      BTFlag = true;
    }
  } else if (BITy->getPointerElementType()->getTypeID() == Type::PointerTyID) {
    PointerType *STyL = cast<PointerType>(BITy->getPointerElementType());
    if (isPointerFloat(STyL)) {
      BTFlag = true;
    }
  } else if (isFloatType(BITy->getPointerElementType())) {
    BTFlag = true;
  }
  return BTFlag;
}

void FPSanitizer::handleLoadProducer(LoadInst *LI, BasicBlock *BB,
    Function *F) {
  Instruction *I = dyn_cast<Instruction>(LI);
  Module *M = F->getParent();
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);

  LLVMContext &C = F->getContext();

  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(C));
  Type *VoidTy = Type::getVoidTy(C);
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());

  Value *Addr = LI->getPointerOperand();
  bool BTFlag = false;

  int fType = 0;
  if (BitCastInst *BI = dyn_cast<BitCastInst>(Addr)) {
    if(checkIfBitcastFromFP(BI)){
      errs()<<"Warning!!! Error provenance might get lost, please rewrite code to avoid load\n";
      errs()<<F->getName()<<":"<<*LI<<"\n";
    }
  }
  if (isFloatType(LI->getType()) || BTFlag) {
    long InsIndex;
    if (isFloat(LI->getType()) || fType == 1) {
      Value *BufAddr = BufAddrMap.at(F);
      FuncInit = M->getOrInsertFunction("updateBuffer", VoidTy, Type::getDoubleTy(M->getContext()), BufAddr->getType());
      Value *DFPVal;
      if(fType == 1){
        DFPVal = IRB.CreateUIToFP(LI, Type::getDoubleTy(M->getContext()), "my_si_fp");
      }
      else{
        DFPVal = IRB.CreateFPExt(LI, Type::getDoubleTy(M->getContext()));
      }
      Value *PtrToInt = IRB.CreatePtrToInt(Addr, Int64Ty, "my_ptr_int");
      Value *FPVal = IRB.CreateUIToFP(PtrToInt,  Type::getDoubleTy(M->getContext()), "my_si_fp");
      BasicBlock *OldBB = I->getParent();
      BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
      createUpdateBlock2(FPVal, DFPVal, BufAddr, I, OldBB, Cont, F);
    } else if (isDouble(LI->getType()) || fType == 2) {
      Value *BufAddr = BufAddrMap.at(F);
      Value *DFPVal;
      if(fType == 2){
        DFPVal = IRB.CreateUIToFP(LI, Type::getDoubleTy(M->getContext()), "my_si_fp");
      }
      else{
        DFPVal = LI;
      }
      Value *PtrToInt = IRB.CreatePtrToInt(Addr, Int64Ty, "my_ptr_int");
      Value *FPVal = IRB.CreateUIToFP(PtrToInt,  Type::getDoubleTy(M->getContext()), "my_si_fp");
      BasicBlock *OldBB = I->getParent();
      BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
      createUpdateBlock2(FPVal, DFPVal, BufAddr, I, OldBB, Cont, F);
    }
  }
}

void FPSanitizer::handleBToFProducer(BitCastInst *SI, BasicBlock *BB,
    Function *F) {
  Instruction *I = dyn_cast<Instruction>(SI);
  Module *M = F->getParent();
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);

  LLVMContext &C = F->getContext();

  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(C));
  Type *VoidTy = Type::getVoidTy(C);
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());

  if (isFloat(SI->getType())) {
    FuncInit = M->getOrInsertFunction("fpsanx_push_value_f", VoidTy, Int32Ty,
        SI->getType());
  } else if (isDouble(SI->getType())) {
    FuncInit = M->getOrInsertFunction("fpsanx_push_value_d", VoidTy, Int32Ty,
        SI->getType());
  }
  Value *BufAddr = BufAddrMap.at(F);
  Value *DFPVal;
  if(isDouble(SI->getType())){
    DFPVal = SI;
  }
  else{
    DFPVal = IRB.CreateFPExt(SI, Type::getDoubleTy(M->getContext()));
  }
  BasicBlock *OldBB = I->getParent();
  BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
  createUpdateBlock(DFPVal, BufAddr, I, OldBB, Cont, F);
}

void FPSanitizer::handleUToFProducer(UIToFPInst *SI, BasicBlock *BB,
    Function *F) {
  Instruction *I = dyn_cast<Instruction>(SI);
  Module *M = F->getParent();
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);

  LLVMContext &C = F->getContext();

  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(C));
  Type *VoidTy = Type::getVoidTy(C);
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());

  if (isFloat(SI->getType())) {
    FuncInit = M->getOrInsertFunction("fpsanx_push_value_f", VoidTy, Int32Ty,
        SI->getType());
  } else if (isDouble(SI->getType())) {
    FuncInit = M->getOrInsertFunction("fpsanx_push_value_d", VoidTy, Int32Ty,
        SI->getType());
  }
  Value *BufAddr = BufAddrMap.at(F);
  //IRB.CreateCall(UpdateBufferFunc, {SI, BufAddr});
  Value *DFPVal;
  if(isDouble(SI->getType())){
    DFPVal = SI;
  }
  else{
    DFPVal = IRB.CreateFPExt(SI, Type::getDoubleTy(M->getContext()));
  }
  BasicBlock *OldBB = I->getParent();
  BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
  createUpdateBlock(DFPVal, BufAddr, I, OldBB, Cont, F);
}

void FPSanitizer::handleSToFProducer(SIToFPInst *SI, BasicBlock *BB,
    Function *F) {
  Instruction *I = dyn_cast<Instruction>(SI);
  Module *M = F->getParent();
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);

  LLVMContext &C = F->getContext();

  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(C));
  Type *VoidTy = Type::getVoidTy(C);
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());

  if (isFloat(SI->getType())) {
    FuncInit = M->getOrInsertFunction("fpsanx_push_value_f", VoidTy, Int32Ty,
        SI->getType());
  } else if (isDouble(SI->getType())) {
    FuncInit = M->getOrInsertFunction("fpsanx_push_value_d", VoidTy, Int32Ty,
        SI->getType());
  }
  Value *BufAddr = BufAddrMap.at(F);
  Value *DFPVal;
  if(isDouble(SI->getType())){
    DFPVal = SI;
  }
  else{
    DFPVal = IRB.CreateFPExt(SI, Type::getDoubleTy(M->getContext()));
  }
  BasicBlock *OldBB = I->getParent();
  BasicBlock *Cont = OldBB->splitBasicBlock(Next, "split");
  createUpdateBlock(DFPVal, BufAddr, I, OldBB, Cont, F);
}

void FPSanitizer::handleUToF(UIToFPInst *SI, BasicBlock *BB, Function *F) {
  AllInstList.push_back(SI);
}

void FPSanitizer::handleSToF(SIToFPInst *SI, BasicBlock *BB, Function *F) {
  AllInstList.push_back(SI);
}

void FPSanitizer::handleBToF(BitCastInst *SI, BasicBlock *BB, Function *F) {
  Value *V = dyn_cast<Value>(SI);
  if(V->getName().startswith("my_bitcast")){
  }
  else{
    AllInstList.push_back(SI);
  }
}

void FPSanitizer::handleLoad(LoadInst *LI, BasicBlock *BB, Function *F) {
  Value *V = dyn_cast<Value>(LI);
  if(V->getName().startswith("my_load_val")){
    return;
  }
  Instruction *I = dyn_cast<Instruction>(LI);
  Module *M = F->getParent();
  Instruction *Next = getNextInstruction(I, BB);
  IRBuilder<> IRB(Next);

  LLVMContext &C = F->getContext();

  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(C));
  Type *VoidTy = Type::getVoidTy(C);
  IntegerType *Int1Ty = Type::getInt1Ty(M->getContext());
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());

  Value *Addr = LI->getPointerOperand();
  ConstantInt *instId = GetInstId(F, I);

  bool BTFlag = false;
  Value *Idx = BufIdxMap.at(F);

  int fType = 0;
  if (isFloatType(LI->getType()) || BTFlag) {
    Value *BOGEP = GEPMap.at(LI);
    long InsIndex;
    Constant *CBTFlagD =
        ConstantInt::get(Type::getInt1Ty(M->getContext()), BTFlag);

    if (isFloat(LI->getType()) || fType == 1) {
      LoadCall = M->getOrInsertFunction("fpsan_load_shadow_f", VoidTy, Int32Ty,
                                        Int64Ty,
                                        Type::getFloatTy(M->getContext()),
                                        MPtrTy, instId->getType());
    } else if (isDouble(LI->getType()) || fType == 2) {
      LoadCall = M->getOrInsertFunction("fpsan_load_shadow_d", VoidTy, Int32Ty,
                                        Int64Ty,
                                        Type::getDoubleTy(M->getContext()),
                                        MPtrTy, instId->getType());
    }

    Value *Addr = readFromBuf(I, BB, F);
    Value *AddrInt = IRB.CreateFPToUI(Addr,  Type::getInt64Ty(M->getContext()), "my_si_fp");

    Value *Val = readFromBuf(I, BB, F);
    if (isFloat(LI->getType()) || fType == 1) {
      Val = IRB.CreateFPTrunc(Val, Type::getFloatTy(M->getContext()));
    }

    Value *LoadI;
    LoadI = IRB.CreateCall(LoadCall, {Idx, AddrInt, Val, BOGEP, instId});

    MInsMap.insert(std::pair<Instruction *, Instruction *>(
        I, dyn_cast<Instruction>(BOGEP)));
    AllInstList.push_back(LI);
  }
}

// handle consumer
void FPSanitizer::handleIns(Instruction *I, BasicBlock *BB, Function *F) {
  // instrument interesting instructions
  if (FPExtInst *FCI = dyn_cast<FPExtInst>(I)) {
    AllInstList.push_back(FCI);
  } else if (UnaryOperator *UO = dyn_cast<UnaryOperator>(I)) {
    switch (UO->getOpcode()) {
    case Instruction::FNeg: {
      handleFNeg(UO, BB, F);
      AllInstList.push_back(I);
    }
    }
  } else if (BitCastInst *SI = dyn_cast<BitCastInst>(I)) {
    handleBToF(SI, BB, F);
  } else if (SIToFPInst *SI = dyn_cast<SIToFPInst>(I)) {
    handleSToF(SI, BB, F);
  } else if (UIToFPInst *UI = dyn_cast<UIToFPInst>(I)) {
    handleUToF(UI, BB, F);
  } else if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
    handleLoad(LI, BB, F);
  } else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
    handleStore(SI, BB, F);
  } else if (SelectInst *SI = dyn_cast<SelectInst>(I)) {
    if (isFloatType(SI->getOperand(1)->getType()) || 
         isFloatType(SI->getOperand(2)->getType())) {
      handleSelect(SI, BB, F);
      AllInstList.push_back(SI);
    }
  } else if (ExtractValueInst *EVI = dyn_cast<ExtractValueInst>(I)) {
  } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(I)) {
    switch (BO->getOpcode()) {
    case Instruction::FAdd:
    case Instruction::FSub:
    case Instruction::FMul:
    case Instruction::FDiv: {
      handleBinOp(BO, BB, F);
      AllInstList.push_back(I);
    }
    }
  } else if (CallInst *CI = dyn_cast<CallInst>(I)) {
    Function *Callee = CI->getCalledFunction();
    CallSite CS(I);
    if (Callee && !CS.isIndirectCall()) {
      if (Callee->getName().startswith("llvm.memcpy")) {
        handleMemCpy(CI, BB, F, Callee->getName());
      }
      else if (Callee->getName().startswith("llvm.memset")) {
        handleMemset(CI, BB, F, Callee->getName());
      }
      else if (isListedFunction(Callee->getName(), "mathFunc.txt")) {
        handleMathLibFunc(CI, BB, F, Callee->getName());
        AllInstList.push_back(I);
      } 
      else if (!Callee->getName().startswith("fpsan")) {
        if (Callee->getName() == "printf") {
          handlePrint(CI, BB, F);
          AllInstList.push_back(CI);
        } 
        else {
          handleCallInst(CI, BB, F);
        }
      }
    } 
    else {
      handleCallInstIndirect(CI, BB, F);
    } //else {
      //errs() << "Warning: return value is treated as constant:" << *I << "\n";
//    }
  }
}

void FPSanitizer::handleBranch(Function *F) {
  if (F->getName().startswith("start_slice") ||
      F->getName().startswith("end_slice") ||
      F->getName().startswith("fpsan")) {
    return;
  }
  Module *M = F->getParent();
  Value *Idx = BufIdxMap.at(F);
  SmallVector<Instruction *, 8> InstList; // Ignore returns cloned.
  for (auto &BB : *F) {
    for (auto &Inst : BB) {
      if (Inst.isTerminator()) {
        if (BranchInst *BI = dyn_cast<BranchInst>(&Inst)) {
          if (BI->isConditional()) {
            IRBuilder<> IRB(BI);
            Value *Cond1 = readFromBuf(BI, &BB, F);
            Value *Cond = IRB.CreateFPToUI(Cond1, Type::getInt1Ty(M->getContext()), "my_trunc");
            FCMPMap.insert(std::pair<Instruction *, Instruction *>(
                  dyn_cast<Instruction>(BI->getOperand(0)),
                  dyn_cast<Instruction>(Cond)));
            BI->setCondition(Cond);
          }
        }
        if (SwitchInst *SI = dyn_cast<SwitchInst>(&Inst)) {
          IRBuilder<> IRB(SI);
          Value *Cond1 = readFromBuf(SI, &BB, F);
          Value *Cond = IRB.CreateFPToUI(Cond1, Type::getInt1Ty(M->getContext()), "my_trunc");
          FCMPMap.insert(std::pair<Instruction *, Instruction *>(
                dyn_cast<Instruction>(SI->getOperand(0)),
                dyn_cast<Instruction>(Cond)));
          if(Cond->getType() != SI->getOperand(0)->getType()){
            Cond = IRB.CreateIntCast(Cond, SI->getOperand(0)->getType(), false, "my_bitcast");
            SI->setCondition(Cond);
          }
          else{
            SI->setCondition(Cond);
          }
        }
      }
    }
  }
}

void FPSanitizer::RemoveEveryThingButFloat(Function *F) {
  if (F->getName().startswith("start_slice") ||
      F->getName().startswith("end_slice") ||
      F->getName().startswith("fpsan")) {
    return;
  }
  Module *M = F->getParent();
  Value *Idx = BufIdxMap.at(F);
  SmallVector<Instruction *, 8> InstList; // Ignore returns cloned.
  for (auto &BB : *F) {
    for (auto &Inst : BB) {
      Value *V = dyn_cast<Value>(&Inst);
      if(V->getName().startswith("my_gep")){
      }
      else if(V->getName().startswith("my_load_idx_c")){
      }
      else if(V->getName().startswith("my_si_fp")){
      }
      else if(V->getName().startswith("my_trunc")){
      }
      else if(V->getName().startswith("my_gep_buf_c")){
      }
      else if(V->getName().startswith("my_load_val_c")){
      }
      else if(V->getName().startswith("my_store_idx_c")){
      }
      else if(V->getName().startswith("my_incr_idx_c")){
      }
      else if(V->getName().startswith("my_alloca")){
      }
      else if(V->getName().startswith("my_func")){
      }
      else if(V->getName().startswith("my_bitcast")){
      }
      else if(V->getName().startswith("arg_alloca")){
      }
      else if(V->getName().startswith("my_phi")){
      }
      else if(V->getName().startswith("my_select")){
      }
      else if (StoreInst *SI = dyn_cast<StoreInst>(&Inst)) {
        Value *SAddr = SI->getPointerOperand();
        if(SAddr->getName().startswith("my_")){
        }
        else{
          InstList.push_back(&Inst);
        }
      }
      else if (CallInst *CI = dyn_cast<CallInst>(&Inst)) {
        Function *Callee = CI->getCalledFunction();
        CallSite CS(&Inst);
        if (CS.isIndirectCall()) {
        }
        else if(Callee && Callee->getName().startswith("pull_print")){
        }
        else if (Callee && (Callee->getName().startswith("fpsan") || 
              Callee->getName().startswith("end_slice") ||
              Callee->getName().startswith("start_slice"))) {
        }
        else if (Callee && LibFuncList.count(Callee->getName()) != 0) {
          InstList.push_back(&Inst);
        }
        else if(OrigFuncMap.count(Callee) == 0){
          InstList.push_back(&Inst);
        }
      }
      else if(!Inst.isTerminator()){
        if(Inst.use_empty()){
          InstList.push_back(&Inst);
        }
        else if(!isFloatType(Inst.getType())){
          Value *V = dyn_cast<Value>(&Inst);
          if(V->getName().startswith("my_gep")){
          }
          else if(V->getName().startswith("my_load_idx_c")){
          }
          else if(V->getName().startswith("my_si_fp")){
          }
          else if(V->getName().startswith("my_gep_buf_c")){
          }
          else if(V->getName().startswith("my_load_val_c")){
          }
          else if(V->getName().startswith("my_store_idx")){
          }
          else if(V->getName().startswith("my_incr_idx")){
          }
          else if(V->getName().startswith("my_alloca")){
          }
          else if(V->getName().startswith("my_func")){
          }
          else if(V->getName().startswith("my_bitcast")){
          }
          else if(V->getName().startswith("arg_alloca")){
          }
          else if(V->getName().startswith("my_phi")){
          }
          else if(V->getName().startswith("my_select")){
          }
          else{
            InstList.push_back(&Inst);
          }
        }
      }
    }
  }
#if 1
  for (Instruction *Inst : InstList) {
    Inst->replaceAllUsesWith(UndefValue::get(Inst->getType()));
    for (auto UI = Inst->user_begin(), UE = Inst->user_end(); UI != UE;) {
      Instruction *I = cast<Instruction>(*UI);
      ++UI;
      I->eraseFromParent();
    }
    Inst->eraseFromParent();
  }
#endif
}

Function *FPSanitizer::addIndexProducer(Function *F) {
  ValueToValueMapTy VMap;

  std::vector<Type *> ArgTypes;

  // Create a new function type...
  Module *M = F->getParent();

  Type *VoidTy = Type::getVoidTy(M->getContext());
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());

  for (const Argument &I : F->args()) {
    ArgTypes.push_back(I.getType());
  }
  // add index argument to shadow slice
  ArgTypes.push_back(Int32Ty);

  FunctionType *FTy = F->getFunctionType();

  std::vector<Type *> Params(FTy->param_begin(), FTy->param_end());
  unsigned NumArgs = Params.size();

  FunctionType *NFTy = FunctionType::get(FTy->getReturnType(), ArgTypes, false);
  // Create the new function body and insert it into the module...
  Function *NewFn =
      Function::Create(NFTy, F->getLinkage(), F->getAddressSpace());
  NewFn->copyAttributesFrom(F);
  NewFn->setComdat(F->getComdat());
  F->getParent()->getFunctionList().insert(F->getIterator(), NewFn);
  NewFn->takeName(F);

  // Loop over all of the callers of the function, transforming the call sites
  // to pass in a larger number of arguments into the new function.
  std::vector<Value *> Args;
  for (Value::user_iterator I = F->user_begin(), E = F->user_end(); I != E;) {
    CallSite CS(*I++);
    if (!CS)
      continue;
    Instruction *Call = CS.getInstruction();
    Function *ParentFun = Call->getParent()->getParent();
    Value *Idx;
    if (CallIdxMap.count(dyn_cast<CallInst>(Call)) != 0) {
      Idx = dyn_cast<Value>(CallIdxMap.at(dyn_cast<CallInst>(Call)));
    } else {
      Idx = BufIdxMap.at(ParentFun);
    }
    Args.assign(CS.arg_begin(), CS.arg_begin() + NumArgs);
    Args.push_back(Idx);
    SmallVector<OperandBundleDef, 1> OpBundles;
    CS.getOperandBundlesAsDefs(OpBundles);

    CallSite NewCS;
    if (InvokeInst *II = dyn_cast<InvokeInst>(Call)) {
      NewCS =
          InvokeInst::Create(NewFn, II->getNormalDest(), II->getUnwindDest(),
                             Args, OpBundles, "", Call);
    } else {
      NewCS = CallInst::Create(NewFn, Args, OpBundles, "", Call);
      cast<CallInst>(NewCS.getInstruction())
          ->setTailCallKind(cast<CallInst>(Call)->getTailCallKind());
    }
    NewCS.setCallingConv(CS.getCallingConv());
    NewCS->setDebugLoc(Call->getDebugLoc());
    uint64_t W;

    if (Call->extractProfTotalWeight(W))
      NewCS->setProfWeight(W);

    Args.clear();

    if (!Call->use_empty())
      Call->replaceAllUsesWith(NewCS.getInstruction());

    NewCS->takeName(Call);
    // Finally, remove the old call from the program, reducing the use-count of F
    Call->eraseFromParent();
  }
  NewFn->getBasicBlockList().splice(NewFn->begin(), F->getBasicBlockList());

  for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(),
                              I2 = NewFn->arg_begin();
       I != E; ++I, ++I2) {
    // Move the name and users over to the new version.
    I->replaceAllUsesWith(&*I2);
    I2->takeName(&*I);
  }
  SmallVector<std::pair<unsigned, MDNode *>, 1> MDs;
  F->getAllMetadata(MDs);
  for (auto MD : MDs)
    NewFn->addMetadata(MD.first, *MD.second);
  // Fix up any BlockAddresses that refer to the function.
  F->replaceAllUsesWith(ConstantExpr::getBitCast(NewFn, F->getType()));
  NewFn->removeDeadConstantUsers();
  F->eraseFromParent();
  return NewFn;
}

Function *FPSanitizer::removeArgs(Function *F, Function *OldF, Value *BufAddr) {
  ValueToValueMapTy VMap;

  std::vector<Type *> ArgTypes;

  // Create a new function type...
  Module *M = F->getParent();
  FunctionType *FTy;
  Type *VoidTy = Type::getVoidTy(M->getContext());

  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M->getContext()));
  IntegerType *Int32Ty = Type::getInt32Ty(M->getContext());
  IntegerType *Int64Ty = Type::getInt64Ty(M->getContext());
  // add index argument to shadow slice
  ArgTypes.push_back(Int32Ty);
  // Create a shadow function with return type as void if original function is
  // of type float. Return is going to be passed as argument.
  FTy = FunctionType::get(VoidTy, ArgTypes, F->getFunctionType()->isVarArg());

  // Create the new function...
  Function *NewFn = Function::Create(FTy, F->getLinkage(), F->getAddressSpace(),
                                     F->getName(), F->getParent());

  Function::iterator Fit = F->begin();
  BasicBlock &BB = *Fit;
  BasicBlock::iterator BBit = BB.begin();
  Instruction *First = &*BBit;
  for (auto &I : BB) {
    if (CallInst *CI = dyn_cast<CallInst>(&I)) {
      Function *Callee = CI->getCalledFunction();
      if (Callee) {
        if (Callee->getName().startswith("start_slice")) {
          First = getNextInstruction(&I, &BB);
        }
      }
    }
  }
  IRBuilder<> IRB(First);
  size_t count = 0;
  std::map<Argument *, Value *> ArgMap;
  for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E;
       ++I) {
    Value *BOGEP;
    AllocaInst *Alloca = IRB.CreateAlloca(ArrayType::get(Real, 1), nullptr, "arg_alloca");
    Value *Indices[] = {ConstantInt::get(Type::getInt32Ty(M->getContext()), 0),
                        ConstantInt::get(Type::getInt32Ty(M->getContext()), 0)};
    BOGEP = IRB.CreateGEP(Alloca, Indices, "my_gep");
    GEPList.push_back(dyn_cast<Instruction>(BOGEP));
    Argument *A = &*I;
    ArgMap.insert(std::pair<Argument *, Value *>(A, BOGEP));
  }

  Argument *Idx;
  for (Function::arg_iterator ait = NewFn->arg_begin(), aend = NewFn->arg_end();
       ait != aend; ++ait) {
    Idx = &*ait;
  }
  {
    BufIdxMap.insert(
        std::pair<Function *, Value *>(NewFn, dyn_cast<Value>(Idx)));
  }

  Instruction *End;
  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (dyn_cast<ReturnInst>(&I)) {
        End = &I;
      }
    }
  }
  IRBuilder<> IRBE(End);
  BasicBlock::iterator BBBit = BB.begin();
  Instruction *FFirst = &*BBBit;
  IRBuilder<> IRBB(FFirst);
  //skip one for return, we don't want to pull float value for return
  int size = 0;
  if (isFloatType(OldF->getFunctionType()->getReturnType())) {
    size = F->arg_size() - 1;
  }
  else{
    size = F->arg_size() ;
  }
  int i = 0;
  CBufAddrMap.insert(std::pair<Function *, Value *>(NewFn, dyn_cast<Value>(BufAddr)));
  for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E;
       ++I) {
    if(i<size){
    Value *BOGEP;
    //    if(I->getType() == MPtrTy){
    FuncInit = M->getOrInsertFunction(
        "fpsanx_pull_value_d", Type::getDoubleTy(M->getContext()), Int32Ty);
    //Value *FloatVal = IRB.CreateCall(FuncInit, {Idx});
    Value *FloatVal = readFromBuf(First, &BB, NewFn);
    Argument *A = &*I;
    BOGEP = ArgMap.at(A);

    ConstantInt *instId = GetInstId(F, FFirst);
    FuncInit =
        M->getOrInsertFunction("fpsanx_init_mpfr", VoidTy, Int32Ty, MPtrTy);
    IRB.CreateCall(FuncInit, {Idx, BOGEP});

    FuncInit = M->getOrInsertFunction(
        "fpsanx_store_tempmeta_dconst_val", VoidTy, Int32Ty, MPtrTy, Type::getDoubleTy(M->getContext()), 
        instId->getType(), Int32Ty);
    ConstantInt *lineNumber = ConstantInt::get(Int32Ty, 0);
    IRB.CreateCall(FuncInit, {Idx, BOGEP, FloatVal, instId, lineNumber});
    FuncInit = M->getOrInsertFunction("fpsanx_clear_mpfr", VoidTy, MPtrTy);
    IRBE.CreateCall(FuncInit, {BOGEP});
    I->replaceAllUsesWith(BOGEP);
    i++;
  }
  }
  NewFn->getBasicBlockList().splice(NewFn->begin(), F->getBasicBlockList());
  NewFn->takeName(F);
  F->eraseFromParent();
  return NewFn;
}

Function *FPSanitizer::cloneFunction(Function *F) {
  ValueToValueMapTy VMap;

  std::vector<Type *> ArgTypes;
  // The user might be deleting arguments to the function by specifying them in
  // the VMap.  If so, we need to not add the arguments to the arg ty vector
  for (const Argument &I : F->args()) {
    if (VMap.count(&I) == 0) // Haven't mapped the argument to anything yet?
      if (isFloatType(I.getType())) {
        ArgTypes.push_back(MPtrTy);
      } else {
        ArgTypes.push_back(I.getType());
      }
  }
  // If Original function returns a float type, add an argument for return in
  // shadow function
  if (isFloatType(F->getFunctionType()->getReturnType())) {
    ArgTypes.push_back(MPtrTy);
  }

  // Create a new function type...
  Module *M = F->getParent();
  FunctionType *FTy;
  Type *VoidTy = Type::getVoidTy(M->getContext());

  // Create a shadow function with return type as void if original function is
  // of type float. Return is going to be passed as argument.
  FTy = FunctionType::get(VoidTy, ArgTypes, F->getFunctionType()->isVarArg());

  // Create the new function...
  std::string NewFuncName = F->getName();
  Function *NewFn = Function::Create(FTy, F->getLinkage(), F->getAddressSpace(),
                                     NewFuncName + "_shadow", F->getParent());
  // Loop over the arguments, copying the names of the mapped arguments over...
  Function::arg_iterator DestI = NewFn->arg_begin();
  for (const Argument &I : F->args()) {
    if (VMap.count(&I) == 0) {     // Is this argument preserved?
      DestI->setName(I.getName()); // Copy the name over...
      VMap[&I] = &*DestI++;        // Add mapping to VMap
    }
  }

  SmallVector<ReturnInst *, 8> Returns; // Ignore returns cloned.
  CloneFunction(NewFn, F, VMap, F->getSubprogram() != nullptr, Returns, "",
                nullptr);
  NewFn->setLinkage(GlobalValue::LinkageTypes::InternalLinkage);
  NewFn->removeAttributes(
      AttributeList::ReturnIndex,
      AttributeFuncs::typeIncompatible(NewFn->getReturnType()));
  NewFn->setCallingConv(CallingConv::Fast);

  // Remove dead arguments. Construct the new parameter list from non-dead
  // arguments. Also construct a new set of parameter attributes to correspond.
  // Skip the first parameter attribute, since that belongs to the return value.

  // Remember which arguments are still alive.
  SmallVector<bool, 10> ArgAlive(FTy->getNumParams(), false);
  SmallVector<AttributeSet, 8> ArgAttrVec;
  const AttributeList &PAL = NewFn->getAttributes();
  bool HasLiveReturnedArg = false;
  std::vector<Type *> Params;
  unsigned i = 0;

  AttributeList NewAttrs = NewFn->getAttributes();
  for (Function::arg_iterator I = NewFn->arg_begin(), E = NewFn->arg_end();
       I != E; ++I, ++i) {
    Argument *A = &*I;
    if (A->getType() == MPtrTy) {
      Params.push_back(A->getType());
      ArgAlive[i] = true;
      ArgAttrVec.push_back(PAL.getParamAttributes(i));
      HasLiveReturnedArg |= PAL.hasParamAttribute(i, Attribute::Returned);
    }
  }

  // Shadow function is always going to return a void type
  FunctionType *NFTy = FunctionType::get(VoidTy, Params, NewFn->isVarArg());
  if (NFTy != NewFn->getFunctionType()) {
    Function *NF =
        Function::Create(NFTy, NewFn->getLinkage(), NewFn->getAddressSpace());
    NF->copyAttributesFrom(NewFn);
    NF->setComdat(NewFn->getComdat());
    NF->setAttributes(NewAttrs);
    NewFn->getParent()->getFunctionList().insert(NewFn->getIterator(), NF);
    NF->takeName(NewFn);

    // Since we have now created the new function, splice the body of the old
    // function right into the new function, leaving the old rotting hulk of the
    // function empty
    i = 0;
    NF->getBasicBlockList().splice(NF->begin(), NewFn->getBasicBlockList());
    for (Function::arg_iterator I = NewFn->arg_begin(), E = NewFn->arg_end(),
                                I2 = NF->arg_begin();
         I != E; ++I, ++i) {
      if (ArgAlive[i]) {
        // If this is a live argument, move the name and users over to the new
        // version.
        I->replaceAllUsesWith(&*I2);
        I2->takeName(&*I);
        ++I2;
      } else {
        // If this argument is dead, replace any uses of it with undef
        // (any non-debug value uses will get removed later on).
        if (!I->getType()->isX86_MMXTy())
          I->replaceAllUsesWith(UndefValue::get(I->getType()));
      }
    }
    NewFn->eraseFromParent();
    return NF;
  } else {
    return NewFn;
  }
}

void FPSanitizer::removeOldInst() {
  for (Instruction *Inst : AllInstList) {
    Inst->replaceAllUsesWith(UndefValue::get(Inst->getType()));
    for (auto UI = Inst->user_begin(), UE = Inst->user_end(); UI != UE;) {
      Instruction *I = cast<Instruction>(*UI);
      ++UI;
      I->eraseFromParent();
    }
    Inst->eraseFromParent();
  }
}
void FPSanitizer::CloneFunctionArgs(
    Function *NewFunc, const Function *OldFunc, ValueToValueMapTy &VMap,
    bool ModuleLevelChanges, SmallVectorImpl<ReturnInst *> &Returns,
    const char *NameSuffix, ClonedCodeInfo *CodeInfo,
    ValueMapTypeRemapper *TypeMapper, ValueMaterializer *Materializer) {
  assert(NameSuffix && "NameSuffix cannot be null!");

  /* Copy all attributes other than those stored in the AttributeList.  We need
   * to remap the parameter indices of the AttributeList.*/
  AttributeList NewAttrs = NewFunc->getAttributes();
  NewFunc->copyAttributesFrom(OldFunc);
  NewFunc->setAttributes(NewAttrs);

  // Fix up the personality function that got copied over.
  if (OldFunc->hasPersonalityFn())
    NewFunc->setPersonalityFn(
        MapValue(OldFunc->getPersonalityFn(), VMap,
                 ModuleLevelChanges ? RF_None : RF_NoModuleLevelChanges,
                 TypeMapper, Materializer));

  SmallVector<AttributeSet, 4> NewArgAttrs(NewFunc->arg_size());
  AttributeList OldAttrs = OldFunc->getAttributes();

  // OldAttrs.remove(AttributeFuncs::typeIncompatible(NewFunNewFuncc->getFunctionType()->getReturnType()));
  NewFunc->setAttributes(
      AttributeList::get(NewFunc->getContext(), OldAttrs.getFnAttributes(),
                         OldAttrs.getRetAttributes(), NewArgAttrs));

  bool MustCloneSP =
      OldFunc->getParent() && OldFunc->getParent() == NewFunc->getParent();
  DISubprogram *SP = OldFunc->getSubprogram();
  if (SP) {
    assert(!MustCloneSP || ModuleLevelChanges);
    // Add mappings for some DebugInfo nodes that we don't want duplicated
    //      even if they're distinct.
    auto &MD = VMap.MD();
    MD[SP->getUnit()].reset(SP->getUnit());
    MD[SP->getType()].reset(SP->getType());
    MD[SP->getFile()].reset(SP->getFile());
    // If we're not cloning into the same module, no need to clone the
    //      subprogram
    if (!MustCloneSP)
      MD[SP].reset(SP);
  }
  SmallVector<std::pair<unsigned, MDNode *>, 1> MDs;
  OldFunc->getAllMetadata(MDs);
  // When we remap instructions, we want to avoid duplicating inlined
  // DISubprograms, so record all subprograms we find as we duplicate
  // instructions and then freeze them in the MD map.
  // We also record information about dbg.value and dbg.declare to avoid
  // duplicating the types.
  DebugInfoFinder DIFinder;

  // Loop over all of the basic blocks in the function, cloning them as
  // appropriate.  Note that we save BE this way in order to handle cloning of
  // recursive functions into themselves.

  for (Function::const_iterator BI = OldFunc->begin(), BE = OldFunc->end();
       BI != BE; ++BI) {
    const BasicBlock &BB = *BI;

    // Create a new basic block and copy instructions into it!
    BasicBlock *CBB = CloneBasicBlock(&BB, VMap, NameSuffix, NewFunc, CodeInfo,
                                      ModuleLevelChanges ? &DIFinder : nullptr);

    // Add basic block mapping.
    VMap[&BB] = CBB;

    // It is only legal to clone a function if a block address within that
    // function is never referenced outside of the function.  Given that, we
    // want to map block addresses from the old function to block addresses in
    // the clone. (This is different from the generic ValueMapper
    // implementation, which generates an invalid blockaddress when
    // cloning a function.)
    if (BB.hasAddressTaken()) {
      Constant *OldBBAddr = BlockAddress::get(const_cast<Function *>(OldFunc),
                                              const_cast<BasicBlock *>(&BB));
      VMap[OldBBAddr] = BlockAddress::get(NewFunc, CBB);
    }
  }

  for (DISubprogram *ISP : DIFinder.subprograms())
    if (ISP != SP)
      VMap.MD()[ISP].reset(ISP);

  for (DICompileUnit *CU : DIFinder.compile_units())
    VMap.MD()[CU].reset(CU);

  for (DIType *Type : DIFinder.types())
    VMap.MD()[Type].reset(Type);

  // Loop over all of the instructions in the function, fixing up operand
  // references as we go.  This uses VMap to do all the hard work.
  for (Function::iterator
           BB = cast<BasicBlock>(VMap[&OldFunc->front()])->getIterator(),
           BE = NewFunc->end();
       BB != BE; ++BB)
    // Loop over all instructions, fixing each one as we find it...
    for (Instruction &II : *BB) {
      RemapInstruction(&II, VMap,
                       ModuleLevelChanges ? RF_None : RF_NoModuleLevelChanges,
                       TypeMapper, Materializer);
    }
  // Change call instruction to call shadow function if available
}

#if 1
void FPSanitizer::CloneFunction(
    Function *NewFunc, const Function *OldFunc, ValueToValueMapTy &VMap,
    bool ModuleLevelChanges, SmallVectorImpl<ReturnInst *> &Returns,
    const char *NameSuffix, ClonedCodeInfo *CodeInfo,
    ValueMapTypeRemapper *TypeMapper, ValueMaterializer *Materializer) {
  assert(NameSuffix && "NameSuffix cannot be null!");

  /* Copy all attributes other than those stored in the AttributeList.  We need
   * to remap the parameter indices of the AttributeList.*/
  AttributeList NewAttrs = NewFunc->getAttributes();
  NewFunc->copyAttributesFrom(OldFunc);
  NewFunc->setAttributes(NewAttrs);

  // Fix up the personality function that got copied over.
  if (OldFunc->hasPersonalityFn())
    NewFunc->setPersonalityFn(
        MapValue(OldFunc->getPersonalityFn(), VMap,
                 ModuleLevelChanges ? RF_None : RF_NoModuleLevelChanges,
                 TypeMapper, Materializer));

  SmallVector<AttributeSet, 4> NewArgAttrs(NewFunc->arg_size());
  AttributeList OldAttrs = OldFunc->getAttributes();

  // OldAttrs.remove(AttributeFuncs::typeIncompatible(NewFunNewFuncc->getFunctionType()->getReturnType()));
  NewFunc->setAttributes(
      AttributeList::get(NewFunc->getContext(), OldAttrs.getFnAttributes(),
                         OldAttrs.getRetAttributes(), NewArgAttrs));

  bool MustCloneSP =
      OldFunc->getParent() && OldFunc->getParent() == NewFunc->getParent();
  DISubprogram *SP = OldFunc->getSubprogram();
  if (SP) {
    assert(!MustCloneSP || ModuleLevelChanges);
    // Add mappings for some DebugInfo nodes that we don't want duplicated
    //      even if they're distinct.
    auto &MD = VMap.MD();
    MD[SP->getUnit()].reset(SP->getUnit());
    MD[SP->getType()].reset(SP->getType());
    MD[SP->getFile()].reset(SP->getFile());
    // If we're not cloning into the same module, no need to clone the
    //      subprogram
    if (!MustCloneSP)
      MD[SP].reset(SP);
  }
  SmallVector<std::pair<unsigned, MDNode *>, 1> MDs;
  OldFunc->getAllMetadata(MDs);
  // When we remap instructions, we want to avoid duplicating inlined
  // DISubprograms, so record all subprograms we find as we duplicate
  // instructions and then freeze them in the MD map.
  // We also record information about dbg.value and dbg.declare to avoid
  // duplicating the types.
  DebugInfoFinder DIFinder;

  // Loop over all of the basic blocks in the function, cloning them as
  // appropriate.  Note that we save BE this way in order to handle cloning of
  // recursive functions into themselves.

  for (Function::const_iterator BI = OldFunc->begin(), BE = OldFunc->end();
       BI != BE; ++BI) {
    const BasicBlock &BB = *BI;

    // Create a new basic block and copy instructions into it!
    BasicBlock *CBB = CloneBasicBlock(&BB, VMap, NameSuffix, NewFunc, CodeInfo,
                                      ModuleLevelChanges ? &DIFinder : nullptr);

    // Add basic block mapping.
    VMap[&BB] = CBB;

    // It is only legal to clone a function if a block address within that
    // function is never referenced outside of the function.  Given that, we
    // want to map block addresses from the old function to block addresses in
    // the clone. (This is different from the generic ValueMapper
    // implementation, which generates an invalid blockaddress when
    // cloning a function.)
    if (BB.hasAddressTaken()) {
      Constant *OldBBAddr = BlockAddress::get(const_cast<Function *>(OldFunc),
                                              const_cast<BasicBlock *>(&BB));
      VMap[OldBBAddr] = BlockAddress::get(NewFunc, CBB);
    }

    // Note return instructions for the caller.
    /*
    if (ReturnInst *RI = dyn_cast<ReturnInst>(CBB->getTerminator())){
      ReturnInst *NewRI = ReturnInst::Create(OldFunc->getContext(), nullptr,
    RI); RI->eraseFromParent(); Returns.push_back(NewRI);
    }
    */
  }

  for (DISubprogram *ISP : DIFinder.subprograms())
    if (ISP != SP)
      VMap.MD()[ISP].reset(ISP);

  for (DICompileUnit *CU : DIFinder.compile_units())
    VMap.MD()[CU].reset(CU);

  for (DIType *Type : DIFinder.types())
    VMap.MD()[Type].reset(Type);

  // Loop over all of the instructions in the function, fixing up operand
  // references as we go.  This uses VMap to do all the hard work.
  for (Function::iterator
           BB = cast<BasicBlock>(VMap[&OldFunc->front()])->getIterator(),
           BE = NewFunc->end();
       BB != BE; ++BB)
    // Loop over all instructions, fixing each one as we find it...
    for (Instruction &II : *BB) {
      RemapInstruction(&II, VMap,
                       ModuleLevelChanges ? RF_None : RF_NoModuleLevelChanges,
                       TypeMapper, Materializer);
    }
  // Change call instruction to call shadow function if available
}

#endif
bool FPSanitizer::runOnModule(Module &M) {

  auto GetTLI = [this](Function &F) -> TargetLibraryInfo & {
    return this->getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
  };

  LLVMContext &C = M.getContext();

  SliceFlag = new GlobalVariable(M, Type::getInt8Ty(M.getContext()), false, GlobalVariable::InternalLinkage, 
      Constant::getNullValue(Type::getInt8Ty(M.getContext())), "my_slice_flag");

  QIdx = new GlobalVariable(M, Type::getInt64Ty(M.getContext()), false, GlobalValue::PrivateLinkage, 
      Constant::getNullValue(Type::getInt64Ty(M.getContext())), "my_global_queue_idx");

  CQIdx = new GlobalVariable(M, Type::getInt64Ty(M.getContext()), false, GlobalVariable::PrivateLinkage, 
      Constant::getNullValue(Type::getInt64Ty(M.getContext())), "my_consumer_queue_idx");
  CQIdx->setThreadLocalMode(GlobalVariable::LocalExecTLSModel);
  //CQIdx->setThreadLocalMode(GlobalVariable::LocalDynamicTLSModel);

  StructType *MPFRTy1 = StructType::create(M.getContext(), "struct.fpsan_mpfr");
  MPFRTy1->setBody(
      {Type::getInt64Ty(M.getContext()), Type::getInt32Ty(M.getContext()),
       Type::getInt64Ty(M.getContext()), Type::getInt64PtrTy(M.getContext())});

  MPFRTy = StructType::create(M.getContext(), "struct.f_mpfr");
  MPFRTy->setBody(llvm::ArrayType::get(MPFRTy1, 1));

  Real = StructType::create(M.getContext(), "struct.temp_entry");
  RealPtr = Real->getPointerTo();
  Real->setBody(
      {MPFRTy, 
       // All the tracing data types start here
       Type::getDoubleTy(M.getContext()), 
       Type::getInt64Ty(M.getContext()),
       Type::getInt32Ty(M.getContext()),
       Type::getInt32Ty(M.getContext()),
       Type::getInt64Ty(M.getContext()), 
       Type::getInt64Ty(M.getContext()),
       Type::getInt64Ty(M.getContext()), 
       Type::getInt64Ty(M.getContext()),
       RealPtr,
       Type::getInt64Ty(M.getContext()), 
       Type::getInt64Ty(M.getContext()),
       RealPtr,
       Type::getInt64Ty(M.getContext())
       });

  MPtrTy = Real->getPointerTo();

  // TODO::Iterate over global arrays to initialize shadow memory
  for (Module::global_iterator GVI = M.global_begin(), E = M.global_end();
       GVI != E;) {
    GlobalVariable *GV = &*GVI++;
    if (GV->hasInitializer()) {
      Constant *Init = GV->getInitializer();
    }
  }
  // Find functions that perform floating point computation. No
  // instrumentation if the function does not perform any FP
  // computations.
  //  const TargetLibraryInfo *TLI;
  for (auto &F : M) {
    auto *TLI = &GetTLI(F);
    LibFunc Func;
    if (!F.hasLocalLinkage() && F.hasName() &&
        TLI->getLibFunc(F.getName(), Func)) {
      LibFuncList.insert(F.getFunction().getName());
    }
    else{
      findInterestingFunctions(&F);
    }
  }

  SmallVector<Function *, 8> FuncList; // Ignore returns cloned.
  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    if (isListedFunction(F.getName(), "forbid.txt"))
      continue;
    if (LibFuncList.count(F.getName()) == 0) {
      FuncList.push_back(&F);
    }
  }

  for (Function *F : FuncList) {
    for (auto &BB : *F) {
      for (auto &I : BB) {
        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          Function *Callee = CI->getCalledFunction();
          if (Callee) {
            if (Callee->getName().startswith("start_slice")) {
              SliceList.push_back(F);
            }
          }
        }
      }
    }
  }
  for (Function *F : FuncList) {
    if (F->isDeclaration())
      continue;
    Function *NewF = cloneFunction(F);

    Function::iterator Fit = NewF->begin();
    BasicBlock &BB = *Fit;
    BasicBlock::iterator BBit = BB.begin();
    Instruction *First = &*BBit;
    IRBuilder<> IRBB(First);
    Value *BufAddr;
    if (CBufAddrMap.count(NewF) == 0) {
      Type *PtrDoubleTy = PointerType::getUnqual(Type::getDoubleTy(M.getContext()));
      FuncInit = M.getOrInsertFunction("fpsanx_get_buf_addr_c",
                                       PtrDoubleTy);
      BufAddr = IRBB.CreateCall(FuncInit, {});
    }
    if (std::find(SliceList.begin(), SliceList.end(), F) != SliceList.end()) {
      // this is the slice, remove arguments
      NewF = removeArgs(NewF, F, BufAddr);
    }
    else{
      CBufAddrMap.insert(std::pair<Function *, Value *>(NewF, dyn_cast<Value>(BufAddr)));
    }

    CloneFuncMap.insert(std::pair<Function *, Function *>(F, NewF));
    OrigFuncMap.insert(std::pair<Function *, Function *>(NewF, F));
    AllOrigFuncList.push_back(F);
    AllFuncList.push_back(NewF);
  }
  // Find list of functions which have start_slice and end_slice.
  // Find list if slices.

  // For each function add fpsanx_get_buf_idx in start of the function. This
  // function will return the index of the buffer where producer and consumer
  // push and pop.
  Type *PtrVoidTy = PointerType::getUnqual(Type::getInt8Ty(M.getContext()));

  for (Function *F : AllOrigFuncList) {
    Function::iterator Fit = F->begin();
    BasicBlock &BB = *Fit;
    BasicBlock::iterator BBit = BB.begin();
    Instruction *First = &*BBit;
    IRBuilder<> IRBB(First);
    if (F->getName().startswith("start_slice") ||
        F->getName().startswith("end_slice")) {
      continue;
    }
    FuncInit = M.getOrInsertFunction("fpsanx_get_buf_idx",
                                     Type::getInt32Ty(M.getContext()), PtrVoidTy, PtrVoidTy);
    Function *ShadowFunc = CloneFuncMap.at(F);
    BitCastInst *OrigF = new BitCastInst(
          F, PointerType::getUnqual(Type::getInt8Ty(M.getContext())), "", First);
    BitCastInst *CloneF = new BitCastInst(
          ShadowFunc, PointerType::getUnqual(Type::getInt8Ty(M.getContext())), "",First);

    Type *PtrDoubleTy = PointerType::getUnqual(Type::getDoubleTy(M.getContext()));
    FuncInit = M.getOrInsertFunction("fpsanx_get_buf_addr", PtrDoubleTy, PtrVoidTy, PtrVoidTy);
    Value *BufAddr = IRBB.CreateCall(FuncInit, {OrigF, CloneF});
    BufAddrMap.insert(std::pair<Function *, Value *>(F, dyn_cast<Value>(BufAddr)));
  }
  for (Function *F : AllFuncList) {
    Function::iterator Fit = F->begin();
    BasicBlock &BB = *Fit;
    BasicBlock::iterator BBit = BB.begin();
    Instruction *First = &*BBit;
    IRBuilder<> IRBB(First);
    if (BufIdxMap.count(F) == 0) {
      if (F->getName().startswith("start_slice") ||
          F->getName().startswith("end_slice")) {
        continue;
      }

      FuncInit = M.getOrInsertFunction("fpsanx_get_buf_idx_c",
                                       Type::getInt32Ty(M.getContext()));
      Value *Idx = IRBB.CreateCall(FuncInit, {});

      BufIdxMap.insert(std::pair<Function *, Value *>(F, dyn_cast<Value>(Idx)));
    }
  }
  for (Function *F : AllFuncList) {
    if (F->getName().startswith("start_slice") ||
        F->getName().startswith("end_slice")) {
      continue;
    }
    handleBranch(F);
  }
  Type *VoidTy = Type::getVoidTy(M.getContext());
  // handle producer: push conditions
  for (Function *F : reverse(AllOrigFuncList)) {
    // TODO handle functions which are not part of slice in producer
    if (F->getName().endswith("main")) {
      //      continue;
    }
    handleSliceFlag(F);
    for (auto &BB : *F) {
      if(BB.getName().startswith("update")){
        continue;
      }
      if (BranchInst *BI = dyn_cast<BranchInst>(BB.getTerminator())) {
        if (BI->isConditional()) {
          if (std::find(AllBranchList.begin(), AllBranchList.end(), dyn_cast<Instruction>(BI)) != AllBranchList.end()) {
            continue;
          }
          Value *Cond = BI->getCondition();
          IRBuilder<> IRB(BI);
          Value *BufAddr = BufAddrMap.at(F);
          Value *FPVal = IRB.CreateUIToFP(Cond,  Type::getDoubleTy(M.getContext()), "my_si_fp");
          BasicBlock *OldBB = BI->getParent();
          BasicBlock *Cont = OldBB->splitBasicBlock(dyn_cast<Instruction>(FPVal)->getNextNode(), "split");
          createUpdateBlock(FPVal, BufAddr, BI, OldBB, Cont, F);
          AllBranchList.push_back(dyn_cast<Instruction>(BI));
          if(isa<FCmpInst>(Cond)){
            FCMPMapPush.insert(std::pair<Instruction *, Instruction *>(
                dyn_cast<Instruction>(Cond), dyn_cast<Instruction>(Cond)));
          } 
        }
      }
      if (SwitchInst *BI = dyn_cast<SwitchInst>(BB.getTerminator())) {
        if (std::find(AllBranchList.begin(), AllBranchList.end(), dyn_cast<Instruction>(BI)) != AllBranchList.end()) {
          continue;
        }
        Value *Cond = BI->getCondition();
        IRBuilder<> IRB(BI);
        Value *BufAddr = BufAddrMap.at(F);
        Value *FPVal = IRB.CreateUIToFP(Cond,  Type::getDoubleTy(M.getContext()), "my_si_fp");
        BasicBlock *OldBB = BI->getParent();
        BasicBlock *Cont = OldBB->splitBasicBlock(dyn_cast<Instruction>(FPVal)->getNextNode(), "split");
        createUpdateBlock(FPVal, BufAddr, BI, OldBB, Cont, F);
        AllBranchList.push_back(dyn_cast<Instruction>(BI));
        if(isa<FCmpInst>(Cond)){
          FCMPMapPush.insert(std::pair<Instruction *, Instruction *>(
                dyn_cast<Instruction>(Cond), dyn_cast<Instruction>(Cond)));
        }
      }
    }

    //handle producer: push fpvalues
    for (auto &BB : *F) {
      if(BB.getName().startswith("update")){
        continue;
      }
      for (auto &I : BB) {
        if(I.getName().startswith("my_"))
          continue;
        if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          Function *Callee = CI->getCalledFunction();
          CallSite CS(&I);
          if (Callee  && !CS.isIndirectCall()) {
            // TODO How to ignore function and library calls?
             if (!Callee->getName().startswith("fpsan") &&
                     !Callee->getName().startswith("printf") &&
                     !Callee->getName().startswith("fprintf") &&
                     !Callee->getName().startswith("start_slice") &&
                     !Callee->getName().startswith("end_slice") &&
                     !Callee->getName().startswith("puts") &&
                     !Callee->getName().startswith("llvm.dbg.")) {

              handleCallInstProducer(CI, &BB, F);
            } 
            else if (Callee->getName().startswith("end_slice")) {
              handleEndSliceCallInstP(CI, F);
            } 
            else if (Callee->getName().startswith("start_slice")) {
              handleStartSliceCallInstP(CI, F);
            }
          }
          else{
            //indirect call
            CallList.push_back(CI);
          }
        } 
        else if (SelectInst *SI = dyn_cast<SelectInst>(&I)) {
          if (isFloatType(SI->getOperand(1)->getType())) {
            handleSelectProducer(SI, &BB, F);
          }
        }
      }
    }

    //handle call inst
    for (CallInst *CI : CallList) {
      BasicBlock *BB = CI->getParent();
      Function *F = BB->getParent();
      handleICallInstProducer(CI, BB, F);
    }
    CallList.clear();

    for (auto &BB : *F) {
      for (auto &I : BB) {
        if (FCmpInst *FCI = dyn_cast<FCmpInst>(&I)) {
          handleFcmpProducer(FCI, &BB, F);
        }
      }
    }
    CondMap.clear();
  }

  // producer ends

  int instId = 0;
  // instrument interesting instructions
  // handle consumer
  Instruction *LastPhi = NULL;
  for (Function *F : reverse(AllFuncList)) {
    // give unique indexes to instructions and instrument with call to
    // dynamic lib
    // create a shadow function for this function
    createMpfrAlloca(F);

    if (F->getName() != "main") {
      // add func_init and func_exit in the start and end of the
      // function to set shadow stack variables
      handleFuncInit(F);
    }
    for (auto &BB : *F) {
      for (auto &I : BB) {
        LLVMContext &instContext = I.getContext();
        ConstantInt *instUniqueId =
            ConstantInt::get(Type::getInt64Ty(M.getContext()), instId);
        ConstantAsMetadata *uniqueId = ConstantAsMetadata::get(instUniqueId);
        MDNode *md = MDNode::get(instContext, uniqueId);
        I.setMetadata("fpsan_inst_id", md);
        instId++;
        if (PHINode *PN = dyn_cast<PHINode>(&I)) {
          if (isFloatType(I.getType())) {
            handlePhi(PN, &BB, F);
            LastPhi = &I;
          }
        }
        handleIns(&I, &BB, F);
      }
    }
    for (auto &BB : *F) {
      for (auto &I : BB) {
        if (F->getName() != "main") {
          if (ReturnInst *RI = dyn_cast<ReturnInst>(&I)) {
            handleReturn(RI, &BB, F);
          }
        }
      }
    }
    for (auto &BB : *F) {
      for (auto &I : BB) {
        if (FCmpInst *FCI = dyn_cast<FCmpInst>(&I)) {
          handleFcmp(FCI, &BB, F);
        }
      }
    }

    for (auto &BB : *F) {
      if (ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
        ReturnInst *NewRI = ReturnInst::Create(F->getContext(), nullptr, RI);
        AllInstList.push_back(RI);
      }
    }

    handleNewPhi(F);
    removeOldInst();

    AllInstList.clear();
    NewPhiMap.clear();
    MInsMap.clear();
    GEPMap.clear();
    ConsMap.clear();
  }

  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    if (F.getName() == "main") {
      // add init and finish func in the start and end of the main function to
      // initialize shadow memory
      handleFuncMainInit(&F);
      handleMainRet(&F);
    } else if (F.getName() == "main_shadow") {
      handleFuncShadowMainInit(&F);
    }
  }

  // Go over all the callers of the slice and remove the call instruction
  for (Function *F : reverse(AllFuncList)) {
    if (std::find(SliceList.begin(), SliceList.end(), F) != SliceList.end()) {
      for (Value::user_iterator I = F->user_begin(), E = F->user_end();
           I != E;) {
        CallSite CS(*I++);
        if (!CS)
          continue;
        Instruction *Call = CS.getInstruction();
        Call->replaceAllUsesWith(UndefValue::get(Call->getType()));
        for (auto UI = Call->user_begin(), UE = Call->user_end(); UI != UE;) {
          Instruction *I = cast<Instruction>(*UI);
          ++UI;
          I->eraseFromParent();
        }
        Call->eraseFromParent();
      }
    }
  }

  for (Function *F : AllFuncList) {
    if (F->getName().startswith("start_slice") ||
        F->getName().startswith("end_slice")) {
      continue;
    }
    RemoveEveryThingButFloat(F);
  }

  for (Instruction *Inst : AllInstList) {
    Inst->replaceAllUsesWith(UndefValue::get(Inst->getType()));
    Inst->eraseFromParent();
  }

  for (Function *F : reverse(AllOrigFuncList)) {
    // handle producer: push fpvalues
    for (auto &BB : *F) {
      if(BB.getName().startswith("update")){
        continue;
      }
      for (auto &I : BB) {
        if(I.getName().startswith("my_"))
          continue;
        if (BitCastInst *SI = dyn_cast<BitCastInst>(&I)) {
          if (isFloatType(SI->getType())) {
            handleBToFProducer(SI, &BB, F);
          }
        } else if (SIToFPInst *SI = dyn_cast<SIToFPInst>(&I)) {
          handleSToFProducer(SI, &BB, F);
        } else if (UIToFPInst *UI = dyn_cast<UIToFPInst>(&I)) {
          handleUToFProducer(UI, &BB, F);
        } else if (CallInst *CI = dyn_cast<CallInst>(&I)) {
          Function *Callee = CI->getCalledFunction();
          CallSite CS(&I);
          if (Callee  && !CS.isIndirectCall()) {
            if (Callee->getName().startswith("llvm.memset")) {
              handleMemsetProducer(CI, &BB, F, Callee->getName());
            }
            if (Callee->getName().startswith("llvm.memcpy")) {
              handleMemCpyProducer(CI, &BB, F, Callee->getName());
            }
          }
        } else if (LoadInst *LI = dyn_cast<LoadInst>(&I)) {
          handleLoadProducer(LI, &BB, F);
        } else if (StoreInst *SI = dyn_cast<StoreInst>(&I)) {
         handleStoreProducer(SI, &BB, F);
        } 
      }
    }
    //Delete load and store for update block and use local variable
    //This should be done once all split blocks have been created
    bool sliceF = false;
    for (auto &BB : *F) {
      createPhiCounter(&BB);
    }
    for (auto &BB : *F) {
      for (auto &I : BB) {
        if(CallInst *CI = dyn_cast<CallInst>(&I)){
          Function *Callee = CI->getCalledFunction();
          if (Callee && Callee->getName().startswith("end_slice")) {
            sliceF = true;
          }
        }
      }
    }
    for (auto &BB : *F) {
      for (auto &I : BB) {
        if (ReturnInst *RI = dyn_cast<ReturnInst>(&I)) {
          if(!sliceF){
            handleStoreIdxProducer(&I, &BB, F);
          }
        }
        else if(CallInst *CI = dyn_cast<CallInst>(&I)){
          Function *Callee = CI->getCalledFunction();
          CallSite CS(&I);
          if(Callee && !CS.isIndirectCall()){
            if (Callee->isDeclaration())
              continue;
            if (!(Callee->getName().startswith("fpsan") || 
                Callee->getName().startswith("start_slice") ||
                Callee->getName().startswith("end_slice"))){
              handleStoreIdxProducerCallInst(&I, &BB, F);
            }
          }
          else {
            handleStoreIdxProducerCallInst(&I, &BB, F);
          }
        }
      }
    }
    
    for (auto &BB : *F) {
      propagatePhiCounter(&BB);
    }
    for (auto &BB : *F) {
      updateUpdateBlock(&BB);
    }
    for (auto &BB : *F) {
      addIncomingPhi(&BB);
    }
    
  }
  QIdxMap.clear();

  //remove cmp block for slices
#if 1
  SmallVector<Instruction *, 8> BrList;
  for (Function *F : reverse(AllOrigFuncList)) {
    bool sliceF = false;
    for (auto &BB : *F) {
      for (auto &I : BB) {
        if(CallInst *CI = dyn_cast<CallInst>(&I)){
          Function *Callee = CI->getCalledFunction();
          if (Callee && Callee->getName().startswith("end_slice")) {
            sliceF = true;
          }
        }
      }
    }
    if(sliceF){
      //1. find a branch instruction to u_cmp in block B
      //2. jump to this block and get the branch instruction in u_cmp block
      //3. get the true block
      //4. create a unconditional branch instruction to the true block from B
      //5. delete u_cmp block
      for (auto &BB : *F) {
        for (auto &I : BB) {
          if (BranchInst *BI = dyn_cast<BranchInst>(&I)) {
            if (!BI->isConditional()) {
              Value *JBB = BI->getOperand(0);
              if(JBB->getName().startswith("u_cmp")){
                for (auto &BBCmp : *F) {
                  if(BBCmp.getName() == JBB->getName()){
                    BranchInst *BranchI = dyn_cast<BranchInst>(BBCmp.getTerminator());
                    BranchInst *BJCmp = BranchInst::Create(BranchI->getSuccessor(0), &BB);
                    BrList.push_back(BI);
                  }
                }
              }
            }
          }
        }
      }
    }
    sliceF = false;
    for (Instruction *Inst : BrList) {
      Inst->eraseFromParent();
    }
    BrList.clear();
  }
#endif

  FuncList.clear();
  for (auto &F : M) {
    if (F.getName().startswith("start_slice") && F.getName().endswith("shadow") ) {
      FuncList.push_back(&F);
    }
  }

  for (Function *F : FuncList) {
    // add buf index to slice_end
    F = addIndexProducer(F);
    // call fpsan_slice_end so that it can push the end token
    handleStartSliceC(F);
  }

  FuncList.clear();
  for (auto &F : M) {
    if (F.getName().startswith("end_slice") && F.getName().endswith("shadow") ) {
      FuncList.push_back(&F);
    }
  }
  for (Function *F : FuncList) {
    // add buf index to slice_end
    F = addIndexProducer(F);
    // call fpsan_slice_end so that it can push the end token
    handleEndSliceC(F);
  }

//  errs()<<"full:"<<M<<"\n";
#endif
  return true;
}

void addFPPass(const PassManagerBuilder &Builder, legacy::PassManagerBase &PM) {
  PM.add(new FPSanitizer());
}

RegisterStandardPasses SOpt(PassManagerBuilder::EP_OptimizerLast, addFPPass);
RegisterStandardPasses S(PassManagerBuilder::EP_EnabledOnOptLevel0, addFPPass);

char FPSanitizer::ID = 0;
static const RegisterPass<FPSanitizer> Y("fpsan", "instrument fp operations",
                                         false, false);

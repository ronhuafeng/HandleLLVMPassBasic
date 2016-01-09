//===- Mypass.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Mypass" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Instrumentation.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/MathExtras.h>
#include <algorithm>
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;
using namespace llvm;

#define DEBUG_TYPE "mypass"

STATISTIC(MypassCounter, "Counts number of functions greeted");


namespace {
    // MyInstrument - Try to construct a pass to instrument log information.
    struct MyInstrument : public FunctionPass {
        static char ID;

        MyInstrument() : FunctionPass(ID) {}

        Function * getPrintF(IRBuilder<> &Builder, Module *M) {

            const char *Name = "printf";
            Function *F = M->getFunction(Name);

            if (!F) {
                GlobalValue::LinkageTypes Linkage = Function::ExternalLinkage;
                FunctionType *Ty = FunctionType::get(Builder.getInt32Ty(), true);
                F = Function::Create(Ty, Linkage, Name, M);
            }

            return F;
        }
        void createPrintF(IRBuilder<> &Builder,
                          std::string Format,
                          ArrayRef<Value *> Values,
                          Module *M) {
            Value *FormatString = Builder.CreateGlobalStringPtr(Format);
            std::vector<Value *> Arguments;

            Arguments.push_back(FormatString);
            Arguments.insert(Arguments.end(), Values.begin(), Values.end());
            Builder.CreateCall(getPrintF(Builder, M), Arguments);
        }

        bool runOnFunction(Function& F) override {
            MypassCounter++;

            // 如果不是 main 函數，就不做處理
            if (F.getName().equals("main") == false)
            {
                return false;
            }


            // 獲取 printf 函數
            Module* m = F.getParent();
            Function* printFun = m->getFunction("printf");
            if (nullptr == printFun)
            {
                errs() << "printf function not get." << '\n';
            }

            ///////////////////////////////////////////
            Module* mod = F.getParent();

            BasicBlock * pb = nullptr;


            errs() << "Function is: " << F.getName() << '\n';

            // 按照如下格式重命名 BasicBlock
            // Name each basic block in the format 'FunctionName_BasicBlockID'
            int count;
            count = 0;
            for (Function::iterator b: F)
            {
                pb = b;
                IRBuilder<> Builder(pb);
                b->setName(F.getName() + "_" + Twine(count));
                //errs() << "Basic Block '" << b->getName() << "'\n";
            }



            // 依次在每個 BasicBlock 處插入打印語句
            count = 0;
            for (Function::iterator b : F)
            {
                pb = b;
                IRBuilder<> Builder(pb);
                Builder.SetInsertPoint(pb, pb->begin());

                std::vector<Value *> ArgsV;

                Twine format(pb->getName());

                format.concat(" %d\n");
                //errs() << format.str() << '\n';
                std::string formatStr = pb->getName().str();
                formatStr += "\t%d \n";

                Value *FormatString = Builder.CreateGlobalStringPtr(formatStr);
                ArgsV.push_back(FormatString);
                ArgsV.push_back(Builder.getInt32(42));
                Builder.CreateCall(getPrintF(Builder, mod), ArgsV);

            }
            return true;
        }
    };
}

char MyInstrument::ID = 0;
static RegisterPass<MyInstrument> Z("MyInstrument", "MyInstrument Pass", false, false);

static void registerMyPass(const PassManagerBuilder &,
                           legacy::PassManagerBase &PM) {
    PM.add(new MyInstrument());
}
static RegisterStandardPasses
        RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
                       registerMyPass);

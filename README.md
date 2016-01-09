# 關於 LLVM Pass 的一些基本操作

記錄一下關於 LLVM Pass 的一些操作，主要是一些配置工作。雖然目前的工作流程不是很優雅，但是可以自定義 LLVM Pass 的處理過程，通過進一步學習可以作出更有意思的東西。

網絡上已經有一下教程了，我也參考了這些教程。

## 使用 clang 加載 LLVM 的 Pass

- [LLVM - Run Own Pass automatically with clang](http://stackoverflow.com/questions/23130821/llvm-run-own-pass-automatically-with-clang)
stack overflow 上的回答，試圖使用 clang 加載 LLVM 的 Pass。
- Polly 库加載 LLVM Pass 的方法也是一个解决思路，我參考了這裏面的很多：[Load Polly into clang and automatically run it at -O3](http://polly.llvm.org/example_load_Polly_into_clang.html)
- 具体的加載過程參考了 Adrian Sampson 博客 [Run an LLVM Pass Automatically with Clang](http://adriansampson.net/blog/clangpass.html) 中提到的操作。

# 工作流程

## 準備工作

將系統中自帶的 llvm 和 clang 都卸載掉，保證 llvm 和 clang 版本的一致性（非常重要），即保證使用的 clang 和 編譯出的 Pass 的動態鏈接庫文件 .so 的一致性。

下載 LLVM 帶有 clang 的源碼，解壓縮到某個目錄，我解壓到了 *~/Projects/llvm-3.7.0.src* 。如果沒有的話可以下載對應版本的 clang 源碼放到 llvm 下的 tools 目錄下面，具體參考這個教程： [Getting Started: Building and Running Clang](http://clang.llvm.org/get_started.html) 。

應該是使用下面的幾個命令編譯下項目：
```shell
mkdir build
cd build
../configure
cmake ..
```
然後執行 `make` 或者 `make -jn` （n 爲並行編譯參數，我的 CPU 是 *4 核 Intel(R) Xeon(R) CPU E3-1225 V2 @ 3.20GHz* ，使用了 `make -j2`，因爲還要同時做其他事情）。

編譯完成後就可以使用 `make install` 進行安裝了。由於 Ubuntu 的庫中自帶 clang 是 3.6 版，所以一開始這裏繞了一些彎路。



## 將一個 C 文件編譯成 LLVM 的 Bitcode

`clang -emit-llvm -c hello.c -o hello.bc`

*hello.c*
```c
#include "stdio.h"

int main()
{
  int a = 0;

  if (a == 1)
    return 0;

  if (a == 2)
    return 0;

  if (a == 3)
    return 0;
  
  return 0;
}
```

## 自定義 LLVM Pass

這部分網絡上有很多教程，就不多說了，教程都比較詳細。

我實現了一個 FunctionPass，主要就是給每個 main 函數中的 BasicBlock 添加上名字，然後插入一條打印指令，打印出這個名字和迷之數字 42 。

代碼放在最後吧，畢竟比較長。

[實現代碼](###PassCode)

最後的註冊 Pass 的部分：

```c++
char MyInstrument::ID = 0;
static RegisterPass<MyInstrument> Z("MyInstrument", "MyInstrument Pass", false, false);

static void registerMyPass(const PassManagerBuilder &, legacy::PassManagerBase &PM) {
  PM.add(new MyInstrument());
}
static RegisterStandardPasses RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible, registerMyPass);
```

## 編譯自定義的 LLVM Pass

由於沒有成功爲 LLVM 項目配置好 [*Clion*(a c++ IDE)](https://www.jetbrains.com/clion/) cmake 的 Makefile （這一點是由於對 cmake 和 makefile 不瞭解的緣故），所以只能採用某種方式繞過。

假設已經把 LLVM 的源碼下載在某個目錄，把自己修改的 Pass 文件複製到 LLVM 源碼樹下 Pass 的對應文件夾，然後編譯。

```shell
cp ~/ClionProjects/LLVMPass/ListBasicBlock/ListBasicBlock.cpp ~/Projects/llvm-3.7.0.src/lib/Transforms/Mypass/Mypass.cpp;
cd ~/Projects/llvm-3.7.0.src/build/lib/Transforms/Mypass/;
make
```

這樣會得到 *~/Projects/llvm-3.7.0.src/build/lib/LLVMMypass.so* 。

## 執行自定義 Pass

```shell
opt -load ~/Projects/llvm-3.7.0.src/build/lib/LLVMMypass.so -MyInstrument <hello.bc -o new_hello.bc
```

## 編譯得到可執行文件

調用 LLVM 的靜態編譯器 llc，`llc new_hello.bc`，然後編譯得到可執行文件 *a.out*，`clang new_hello.s` 。

## 執行可執行文件

`./a.out` 可以得到輸出

```
main_0	42 
main_02	42 
main_04	42 
main_06	42 
main_07	42 

```

到此爲止就基本結束了，至於如果按照自定義需求修改 Pass 是就要繼續學習了。

---

## 附錄

###<a name="PassCode"/>實現代碼

```c++
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

            if (F.getName().equals("main") == false)
            {
                return false;
            }

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



            count = 0;
            for (Function::iterator b : F)
            {
                pb = b;
                IRBuilder<> Builder(pb);
                Builder.SetInsertPoint(pb, pb->begin());



//                ConstantInt* const_int_6 = ConstantInt::get(mod->getContext(), APInt(32, StringRef("6"), 10));
                std::vector<Value *> ArgsV;
//                ArgsV.push_back(const_int_6);
                // print basicblock id number
                // errs() << "Number " << count << ":\t" << *b << '\n';

//                AllocaInst* c = Builder.CreateAlloca(Type::getInt32Ty(F.getContext()), nullptr, "c");
//                c->setAlignment(4);

//                Builder.CreateAlignedStore(const_int_6, c, 4);
//                Value* CurC = Builder.CreateAlignedLoad(c, 4);
//                Value* NextC = Builder.CreateAdd(CurC, Builder.getInt32(1));
//                Builder.CreateAlignedStore(NextC, c, 4);

//                ArgsV.push_back(NextC);

                Twine format(pb->getName());

                format.concat(" %d\n");
                //errs() << format.str() << '\n';
                std::string formatStr = pb->getName().str();
                formatStr += "\t%d \n";

                Value *FormatString = Builder.CreateGlobalStringPtr(formatStr);
                ArgsV.push_back(FormatString);
                ArgsV.push_back(Builder.getInt32(42));
                Builder.CreateCall(getPrintF(Builder, mod), ArgsV);
                //createPrintF(Builder, "Run ", ArgsV, mod);

                //errs() << count << ":\t" << *b << '\n';
                //return true;
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




```

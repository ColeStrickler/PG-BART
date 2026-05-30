#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"

// Correct locations for LLVM 23
#include "llvm/IR/PassManager.h"      // ← This is the correct one
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/IRBuilder.h"

// Support
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
using namespace llvm;
#include <iostream>
namespace {


#define BB_COUNT_THRESHOLD 6
#define INST_COUNT_THRESHOLD 100

struct IPCAnalysisPass : PassInfoMixin<IPCAnalysisPass> {


    unsigned int GetFunctionInstCount(Function& F) {
        unsigned int ret  = 0;
        for (auto& BB: F) {
            ret += BB.size();
        }
        return ret;
    }


    bool FilterNonUser(Function& F) {
        StringRef Name = F.getName();
         // ==================== STRONG LIBRARY FILTER ====================
        if (F.isDeclaration() || F.isIntrinsic())
            return true;
        // Skip anything from std:: namespace
        if (Name.starts_with("_ZSt") ||           // std:: (most common)
            Name.starts_with("_ZNSt") ||          // std:: in namespaces
            Name.starts_with("_ZNKS") ||
            Name.starts_with("std::") ||          // demangled
            Name.starts_with("__") ||
            Name.contains("__cxx11") ||           // std::__cxx11::basic_string
            Name.contains("__detail") ||          // std::__detail:: (hashtable internals)
            Name.contains("_Hashtable") ||        // _Hashtable stuff
            Name.contains("__gnu_cxx"))
            return true;
        // Additional common C++ std internals
        if (Name.contains("basic_string") ||
            Name.contains("vector<") ||
            Name.contains("allocator<") ||
            Name.contains("operator new") ||
            Name.contains("operator delete"))
            return true;
        return false;
    }



    bool FunctionHasLoop(FunctionAnalysisManager& FAM, Function& f) {
         // === Get LoopInfo for this function ===
        LoopInfo &LI = FAM.getResult<LoopAnalysis>(f);

        bool hasLoop = !LI.empty();   // or !LI.begin() == LI.end()
        return hasLoop;
    }


    bool SkipFunction(FunctionAnalysisManager& FAM, Function& F) {
        unsigned int func_bb_count = F.size();
        unsigned int func_inst_count = GetFunctionInstCount(F);
        return !(FunctionHasLoop(FAM, F)) && (func_bb_count <= BB_COUNT_THRESHOLD || func_inst_count <= INST_COUNT_THRESHOLD);
    }


    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
        LLVMContext &Ctx = M.getContext();

        FunctionCallee RecordEnterFn = CreateFuncEnterCallbackRef(M);
        FunctionCallee RecordExitFn = CreateFuncExitCallbackRef(M);

        
        FunctionAnalysisManager &FAM = MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

        for (Function &F : M) {
            std::string func_name = F.getName().str();
           

            if (func_name == "ipcfunc_enter")
                continue;
            if (func_name == "ipcfunc_exit")
                continue;

            if (FilterNonUser(F))
                continue;

            if (SkipFunction(FAM, F))
                continue;



            printf("Doing function %s\n", func_name.c_str());
            IRBuilder<> Builder(F.getContext());
            BasicBlock &Entry = F.getEntryBlock();
            Builder.SetInsertPoint(Entry.getFirstNonPHIOrDbgOrLifetime());
            Builder.CreateCall(RecordEnterFn, {  });

            for (BasicBlock &BB : F) {
                for (Instruction &I : BB) {
                    if (auto *Ret = dyn_cast<ReturnInst>(&I)) {
                        Builder.SetInsertPoint(Ret);        // insert BEFORE ret
                        Builder.CreateCall(RecordExitFn, {});
                    }
                }
            }
        }



        return PreservedAnalyses::all();
    }
    

    FunctionCallee CreateFuncEnterCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "ipcfunc_enter",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {

                },
                false
            )
        );
    }

    FunctionCallee CreateFuncExitCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "ipcfunc_exit",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {

                },
                false
            )
        );
    }

};


}

// REQUIRED LLVM 14 plugin entry point
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(void) {

    return {
        LLVM_PLUGIN_API_VERSION,
        "IPCAnalysisPass",
        LLVM_VERSION_STRING,

        // registration callback
        [](PassBuilder &PB) {

            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {

                    if (Name == "IPCAnalysisPass") {
                        MPM.addPass(IPCAnalysisPass());
                        return true;
                    }

                    return false;
                });
        }
    };
}



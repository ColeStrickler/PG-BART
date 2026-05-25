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

namespace {

struct InstructionAnalysisPass : PassInfoMixin<InstructionAnalysisPass> {
    uint64_t BBIDCounter = 0;
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
        LLVMContext &Ctx = M.getContext();

        FunctionCallee BBFn = CreateBBFunctionCallbackRef(M);
        FunctionCallee FuncExitFn = CreateFuncExitFunctionCallbackRef(M);
        FunctionCallee FuncEntryFn = CreateFuncEntryFunctionCallbackRef(M);

        for (Function &F : M) {
            std::string func_name = F.getName().str();

            if (func_name == "bb_entry_callback")
                continue;
            if (func_name == "function_entry_callback")
                continue;
            if (func_name == "function_exit_callback")
                continue;



                // Skip our own instrumentation functions
            if (func_name == "bb_entry_callback" ||
                func_name == "function_entry_callback" ||
                func_name == "function_exit_callback")
                continue;

            // Skip library / std functions
            if (F.isDeclaration() || F.isIntrinsic()) {
                continue;
            }

            if (func_name.rfind("_ZSt", 0) == 0 ||      // std:: 
                func_name.rfind("_ZNSt", 0) == 0 || 
                func_name.rfind("std::", 0) == 0 ||
                func_name.find("__gnu_cxx") != std::string::npos ||
                func_name.find("operator new") != std::string::npos ||
                func_name.find("operator delete") != std::string::npos) {
                continue;
            }


                IRBuilder<> EntryBuilder(&*F.getEntryBlock().getFirstInsertionPt());
            EntryBuilder.CreateCall(FuncEntryFn, {});

            for (BasicBlock &BB : F) {
                

                // Exit - before every return
                if (ReturnInst *Ret = dyn_cast<ReturnInst>(BB.getTerminator())) {
                    IRBuilder<> ExitBuilder(Ret);
                    ExitBuilder.CreateCall(FuncExitFn, {});
                }

                uint64_t BBID = BBIDCounter++;

                // Count instructions in this basic block
                uint64_t numInstructions = 0;
                for (Instruction &I : BB) {
                    numInstructions++;
                }

                IRBuilder<> Builder(&*BB.getFirstInsertionPt());

                Value *BBIDVal =
                    Builder.getInt64(BBID);

                Value *numInstVal =
                    Builder.getInt64(BBID);

               Builder.CreateCall(BBFn, { numInstVal });



               
            }
        }

        return PreservedAnalyses::all();
    }

    FunctionCallee CreateBBFunctionCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "bb_entry_callback",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {
                    Type::getInt64Ty(Ctx)
                },
                false
            )
        );
    }


    FunctionCallee CreateFuncEntryFunctionCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "function_entry_callback",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {
                },
                false
            )
        );
    }

    FunctionCallee CreateFuncExitFunctionCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "function_exit_callback",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {
                },
                false
            )
        );
    }



    
    FunctionCallee CreateRuntimeInitFunctionCallbackRef(Module& M) {
        LLVMContext &Ctx = M.getContext();
        return M.getOrInsertFunction(
            "runtime_init",
            FunctionType::get(
                Type::getVoidTy(Ctx),
                {
                },
                false
            )
        );
    }


    void InstallRuntimeInit(Module& M, llvm::Function& mainFunc)
    {
        FunctionCallee RuntimeInitFn = CreateRuntimeInitFunctionCallbackRef(M);
        BasicBlock &EntryBB = mainFunc.getEntryBlock();
        IRBuilder<> Builder(&*EntryBB.begin());

        Builder.CreateCall(RuntimeInitFn);
    }

};

} // namespace


// REQUIRED LLVM 14 plugin entry point
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo(void) {

    return {
        LLVM_PLUGIN_API_VERSION,
        "InstructionAnalysisPass",
        LLVM_VERSION_STRING,

        // registration callback
        [](PassBuilder &PB) {

            PB.registerPipelineParsingCallback(
                [](StringRef Name,
                   ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {

                    if (Name == "InstructionAnalysisPass") {
                        MPM.addPass(InstructionAnalysisPass());
                        return true;
                    }

                    return false;
                });
        }
    };
}
#include "TypeScript/TypeScriptExceptionPass.h"

#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/ADT/SmallPtrSet.h"

using namespace llvm;
using namespace PatternMatch;

#define DEBUG_TYPE "pass"

namespace
{

struct CatchRegion
{
    CatchRegion() = default;

    LandingPadInst *landingPad;
    llvm::SmallVector<CallBase *> calls;
    CatchPadInst *catchPad;
    StoreInst *store;
    InvokeInst *unwindInfoOp;
    Value *stack;
    bool hasAlloca;
    llvm::Instruction *end;
    bool isCleanup;
};

struct TypeScriptExceptionPass : public FunctionPass
{
    static char ID;
    TypeScriptExceptionPass() : FunctionPass(ID)
    {
    }

    bool runOnFunction(Function &F) override
    {
        auto MadeChange = false;

        llvm::SmallVector<CatchRegion> catchRegionsWorkSet;

        LLVM_DEBUG(llvm::dbgs() << "\nFunction: " << F.getName() << "\n\n";);
        LLVM_DEBUG(llvm::dbgs() << "\nDump Before: " << F << "\n\n";);

        CatchRegion *catchRegion = nullptr;
        auto endOfCatch = false;
        llvm::SmallVector<llvm::Instruction *> toRemoveWorkSet;
        for (auto &I : instructions(F))
        {
            if (auto *LPI = dyn_cast<LandingPadInst>(&I))
            {
                catchRegionsWorkSet.push_back(CatchRegion());
                catchRegion = &catchRegionsWorkSet.back();

                catchRegion->landingPad = LPI;

                endOfCatch = false;
                continue;
            }

            // it is outsize of catch/finally region
            if (!catchRegion)
            {
                continue;
            }

            if (endOfCatch)
            {
                // BR, or instraction without BR
                catchRegion->end = &I;
                endOfCatch = false;
                catchRegion = nullptr;
                continue;
            }

            if (catchRegion->unwindInfoOp == nullptr)
            {
                if (auto *II = dyn_cast<InvokeInst>(&I))
                {
                    catchRegion->unwindInfoOp = II;
                }
            }

            if (auto *SI = dyn_cast<StoreInst>(&I))
            {
                assert(!catchRegion->store);
                catchRegion->store = SI;
            }

            if (auto *AI = dyn_cast<AllocaInst>(&I))
            {
                catchRegion->hasAlloca = true;
            }

            if (auto *CI = dyn_cast<CallInst>(&I))
            {
                LLVM_DEBUG(llvm::dbgs() << "\nCall: " << CI->getCalledFunction()->getName() << "");

                if (CI->getCalledFunction()->getName() == "__cxa_end_catch")
                {
                    toRemoveWorkSet.push_back(&I);
                    endOfCatch = true;
                    continue;
                }
            }

            if (auto *II = dyn_cast<InvokeInst>(&I))
            {
                LLVM_DEBUG(llvm::dbgs() << "\nInvoke: " << II->getCalledFunction()->getName() << "");

                if (II->getCalledFunction()->getName() == "__cxa_end_catch")
                {
                    toRemoveWorkSet.push_back(&I);
                    catchRegion->end = &I;
                    continue;
                }
            }

            if (auto *CB = dyn_cast<CallBase>(&I))
            {
                catchRegion->calls.push_back(CB);
                continue;
            }
        }

        // create begin of catch block
        for (auto &catchRegion : catchRegionsWorkSet)
        {
            auto *LPI = catchRegion.landingPad;

            LLVM_DEBUG(llvm::dbgs() << "\nProcessing: " << *LPI << " isKnownSentinel: " << (LPI->isKnownSentinel() ? "true" : "false")
                                    << "\n\n";);

            // add catchswitch & catchpad
            llvm::IRBuilder<> Builder(LPI);
            llvm::LLVMContext &Ctx = Builder.getContext();

            // split
            BasicBlock *CurrentBB = LPI->getParent();
            BasicBlock *ContinuationBB = CurrentBB->splitBasicBlock(LPI->getIterator(), "catch");

            CurrentBB->getTerminator()->eraseFromParent();

            auto *II = catchRegion.unwindInfoOp;
            auto *CSI = CatchSwitchInst::Create(ConstantTokenNone::get(Ctx),
                                                II ? II->getUnwindDest() : nullptr
                                                /*unwind to caller if null*/,
                                                1, "catch.switch", CurrentBB);
            CSI->addHandler(ContinuationBB);

            CatchPadInst *CPI = nullptr;
            if (LPI->getNumClauses() > 0 && LPI->isCatch(0))
            {
                // check what is type of catch
                auto value = LPI->getOperand(0);
                auto isNullInst = isa<ConstantPointerNull>(value);
                if (isNullInst)
                {
                    // catch (...) as catch value is null
                    auto nullI8Ptr = ConstantPointerNull::get(PointerType::get(IntegerType::get(Ctx, 8), 0));
                    auto iVal64 = ConstantInt::get(IntegerType::get(Ctx, 32), 64);
                    CPI = CatchPadInst::Create(CSI, {nullI8Ptr, iVal64, nullI8Ptr}, "catchpad", LPI);
                }
                else
                {
                    auto varRef = catchRegion.store;
                    assert(varRef);
                    auto iValTypeId = ConstantInt::get(IntegerType::get(Ctx, 32), getTypeNumber(varRef->getPointerOperandType()));
                    CPI = CatchPadInst::Create(CSI, {value, iValTypeId, varRef->getPointerOperand()}, "catchpad", LPI);
                    varRef->eraseFromParent();
                }
            }
            else
            {
                llvm_unreachable("not implemented");
            }

            // save stack
            Value *SP = nullptr;
            if (catchRegion.hasAlloca)
            {
                SP = Builder.CreateCall(Intrinsic::getDeclaration(F.getParent(), Intrinsic::stacksave), {});
                catchRegion.stack = SP;
            }

            toRemoveWorkSet.push_back(&*LPI);
            catchRegion.catchPad = CPI;

            // set funcset
            for (auto callBase : catchRegion.calls)
            {
                llvm::SmallVector<OperandBundleDef> opBundle;
                opBundle.emplace_back(OperandBundleDef("funclet", CPI));
                auto *newCallBase = CallBase::Create(callBase, opBundle, callBase);
                callBase->replaceAllUsesWith(newCallBase);
                callBase->eraseFromParent();
            }

            // LLVM_DEBUG(llvm::dbgs() << "\nLanding Pad - Done. Function Dump: " << F << "\n\n";);

            MadeChange = true;
        }

        // create end of catch block
        for (auto &catchRegion : catchRegionsWorkSet)
        {
            auto *I = catchRegion.end;
            auto *LPI = catchRegion.landingPad;
            assert(LPI);

            llvm::BasicBlock *retBlock = nullptr;

            llvm::IRBuilder<> Builder(I);

            auto *BI = dyn_cast<BranchInst>(I);
            if (BI)
            {
                retBlock = BI->getSuccessor(0);
            }
            else if (auto *II = dyn_cast<InvokeInst>(I))
            {
                retBlock = II->getNormalDest();
            }
            else
            {
                retBlock = Builder.GetInsertBlock()->splitBasicBlock(I, "end.of.exception");
                BI = dyn_cast<BranchInst>(&retBlock->getPrevNode()->back());
                Builder.SetInsertPoint(BI);
            }

            if (catchRegion.hasAlloca)
            {
                assert(catchRegion.stack);
                // restore stack
                Builder.CreateCall(Intrinsic::getDeclaration(F.getParent(), Intrinsic::stackrestore), {catchRegion.stack});
            }

            assert(catchRegion.catchPad);

            auto CR = CatchReturnInst::Create(catchRegion.catchPad, retBlock, BI ? BI->getParent() : I->getParent());

            if (BI)
            {
                // remove BranchInst
                BI->replaceAllUsesWith(CR);
                toRemoveWorkSet.push_back(&*BI);
            }

            // LLVM_DEBUG(llvm::dbgs() << "\nTerminator after: " << *RI->getParent()->getTerminator() << "\n\n";);
            // LLVM_DEBUG(llvm::dbgs() << "\nResume - Done. Function Dump: " << F << "\n\n";);

            MadeChange = true;
        }

        // remove
        for (auto CI : toRemoveWorkSet)
        {
            CI->eraseFromParent();
        }

        // LLVM_DEBUG(llvm::dbgs() << "\nDone. Function Dump: " << F << "\n\n";);

        LLVM_DEBUG(llvm::dbgs() << "\nChange: " << MadeChange << "\n\n";);
        LLVM_DEBUG(llvm::dbgs() << "\nDump After: " << F << "\n\n";);

        return MadeChange;
    }

    int getTypeNumber(Type *catchValType)
    {
        if (catchValType->isIntegerTy() || catchValType->isFloatTy())
        {
            return 0;
        }

        // default if char*, class etc
        return 1;
    }
};
} // namespace

char TypeScriptExceptionPass::ID = 0;

#define CONFIG false
#define ANALYSIS false

INITIALIZE_PASS(TypeScriptExceptionPass, DEBUG_TYPE, TYPESCRIPT_EXCEPTION_PASS_NAME, CONFIG, ANALYSIS)

static RegisterPass<TypeScriptExceptionPass> X(DEBUG_TYPE, TYPESCRIPT_EXCEPTION_PASS_NAME, CONFIG, ANALYSIS);

const void *llvm::getTypeScriptExceptionPassID()
{
    return &TypeScriptExceptionPass::ID;
}
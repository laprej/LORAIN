//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hello"
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Type.h"

#include <map>


// COPIED FROM PREVIOUS ATTEMPT
#include "llvm/Pass.h"
#include "llvm/Module.h"
#include "llvm/Function.h"
#include "llvm/LLVMContext.h"
#include "llvm/Value.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/InstVisitor.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ValueSymbolTable.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/ADT/PostOrderIterator.h"

#include "llvm/Assembly/Writer.h"

#include <stack>
#include <set>
#include <list>



using namespace llvm;



namespace {
    static cl::opt<std::string> OverFunc("rev-func",
										 cl::desc("<func to reverse>"));//, llvm::cl::Required);	
	
	static cl::opt<std::string> TargetFunc("tgt-func",
										   cl::desc("<func to output>"));//, llvm::cl::Required);
    
    class Inverter : public InstVisitor<Inverter>
    {
    public:
        
        IRBuilder<> *builder;
        
        void visitStoreInst(StoreInst &I) {
            errs() << "STORE INSTRUCTION\n";
            
            Value *storeVal = I.getPointerOperand();
            if (GlobalValue *gv = dyn_cast<GlobalValue>(storeVal)) {
                errs() << storeVal->getName() << " is a global value\n";
            }
            else {
                errs() << storeVal->getName() << " is not a global value\n";
                return;
                // FIX THIS LATER
            }
            
            //builder.Cr
        }
    };
    
    // Hello - The first implementation, without getAnalysisUsage.
    struct Hello : public ModulePass {
        static char ID; // Pass identification, replacement for typeid
        Hello() : ModulePass(ID) {}
        
        /// Our Inverter class
        Inverter *inv;
        
        std::map<BasicBlock *, BasicBlock *> newToOld;
        std::map<BasicBlock *, BasicBlock *> oldToNew;
        
        IRBuilder<> *builder;
        
        BasicBlock *reverseBlock(BasicBlock *B)
        {
            BasicBlock::iterator it = B->end();
            BasicBlock::iterator E = B->begin();
            
            /*
             BasicBlock *block = BasicBlock::Create(getGlobalContext(),
             "rev_bb", reverse);
             */
            
            errs() << "BARBAR\n";
            --it;
            while (it) {
                static int count = 0;
                //errs() << count++ << ": " << *it << "\n";
                //if (StoreInst *i = dyn_cast<StoreInst>(it)) {
                if (Instruction *i = dyn_cast<Instruction>(it)) {
                    /* if (isa<UnreachableInst>(i)) {
                     // Skip the unreachables...
                     continue;
                     } */
                    IRBuilder<> build();
                    errs() << "reversing " << *i << " instruction\n";
                    inv->visit(*i);
                }
                if (it != E) {
                    --it;
                }
                else {
                    break;
                }
            }
            errs() << "\\BARBAR\n";
        }
        
        Function *createReverseFunction(Module &M)
        {
            Constant *temp = M.getOrInsertFunction(TargetFunc, 
												   Type::getVoidTy(M.getContext()),
												   (Type *)0);
            
            if (!temp) {
				DEBUG(errs() << "We have a problem\n");
				exit(-1);
			}
			
			Function *reverse = cast<Function>(temp);
			if (reverse) {
				DEBUG(errs() << "OK supposedly we inserted " << TargetFunc << "\n");
			}
			else {
				DEBUG(errs() << "Problem inserting function " << TargetFunc << "\n");
				exit(-1);
			}
            
            return reverse;
        }
        
        
        /**
         This function assumes that the IR is single-exit so we have to
         run that pass first
         */
        BasicBlock *findExitBlock(Function &F)
        {
            Function::iterator it, E;
            
            BasicBlock *ret = 0;
            
            for (it = F.begin(), E = F.end(); it != E; ++it) {
                BasicBlock *b = it;
                
                if (isa<ReturnInst>(b->getTerminator())) {
                    DEBUG(errs() << "We found the return instruction!\n");
                    DEBUG(errs() << "findExitBlock returning: " << *b);
                    if (ret) {
                        errs() << "Error: multiple function exits found\n";
                        errs() << "use the -mergereturn opt pass\n";
                        exit(-1);
                    }
                    ret = b;
                }
            }
            
            assert(ret && "No return instruction found!");
            
            return ret;
        }
        
        void reverseBlockInto(BasicBlock *oldBB, BasicBlock *newBB)
        {
            std::vector<BasicBlock*> pred_list;
            
            errs() << "We're in reverseBlockInto\n";
            
            int count = 0;
            
            for (pred_iterator PI = pred_begin(oldBB), E = pred_end(oldBB); PI != E; ++PI) {
                BasicBlock *Pred = *PI;
                errs() << "pred of " << *oldBB << ": " << *Pred << "\n";
                // ...
                pred_list.push_back(Pred);
                
                count++;
            }
            
            if (count == 0) {
                // No predecessors means this must be have been the entry.  Now it's the exit
                
                //
                reverseBlock(oldBB);
                //
                
                errs() << "count is zero\n";
                IRBuilder<> builder(newBB);
                builder.CreateRetVoid();
            }
            
            if (count == 1) {
                // Straight-through
                
                //
                reverseBlock(oldBB);
                //
                
                errs() << "count is one\n";
                IRBuilder<> builder(newBB);
                BasicBlock *key = pred_list[0];
                errs() << "Our key is " << *key << "\n";
                BasicBlock *data = oldToNew[key];
                errs() << "Our data is " << *data << "\n";
                builder.CreateBr(data);
            }
            
            if (count == 2) {
                // Merge point
                errs() << "count is two\n";
            }
            
            if (count > 2) {
                // Can't do this yet
                exit(-1);
            }
        }
        
        
        virtual bool runOnModule(Module &M) {
            if (Function *rev = M.getFunction(OverFunc)) {
                DEBUG(errs() << "Found " << OverFunc << "\n");
                
                Function *target = createReverseFunction(M);
                
                // Find the exit block from OverFunc
                BasicBlock *oldBB = findExitBlock(*rev);
                BasicBlock *newEntryBB = BasicBlock::Create(M.getContext(), "r_" + oldBB->getName(), target);
                newToOld.insert(std::make_pair(newEntryBB, oldBB));
                
                // Create all the new (empty) BBs
                for (Function::iterator i = rev->begin(), e = rev->end(); i != e; ++i) {
                    BasicBlock *bb = i;
                    if (bb == oldBB) {
                        continue;
                    }
                    errs() << "BB info: " << i->getName() << "\n";
                    BasicBlock *newBB = BasicBlock::Create(M.getContext(), "r_" + i->getName(), target);
                    errs() << "Reverse BB info: " << newBB->getName() << "\n";
                    newToOld.insert(std::make_pair(newBB, bb));
                    oldToNew.insert(std::make_pair(bb, newBB));
                }
                
                for (std::map<BasicBlock *, BasicBlock *>::iterator it = newToOld.begin(), e = newToOld.end();
                     it != e; ++it) {
                    errs() << *(*it).first << ": " << *(*it).second << "\n";
                }
                
                errs() << "**********************************\n\n";
                
                for (std::map<BasicBlock *, BasicBlock *>::iterator it = newToOld.begin(), e = newToOld.end(); 
                     it != e; ++it) {
                    //errs() << *(*it).first << ": " << *(*it).second << "\n";
                    reverseBlockInto((*it).second, (*it).first);
                }
                
                
            }
            return true;
        }
    };
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");

//namespace {
//  // Hello2 - The second implementation with getAnalysisUsage implemented.
//  struct Hello2 : public FunctionPass {
//    static char ID; // Pass identification, replacement for typeid
//    Hello2() : FunctionPass(ID) {}
//
//    virtual bool runOnFunction(Function &F) {
//      ++HelloCounter;
//      errs() << "Hello: ";
//      errs().write_escaped(F.getName()) << '\n';
//      return false;
//    }
//
//    // We don't modify the program, so we preserve all analyses
//    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
//      AU.setPreservesAll();
//    }
//  };
//}
//
//char Hello2::ID = 0;
//static RegisterPass<Hello2>
//Y("hello2", "Hello World Pass (with getAnalysisUsage implemented)");

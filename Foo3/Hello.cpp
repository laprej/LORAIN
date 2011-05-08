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

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/ADT/ilist.h"


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
    
    class Swapper {
        
    };
    
    // Hello - The first implementation, without getAnalysisUsage.
    struct Hello : public ModulePass {
        static char ID; // Pass identification, replacement for typeid
        Hello() : ModulePass(ID) {
            builder = 0;
        }
        
        /// Our Inverter class
        Inverter *inv;
        
        std::list<Instruction*> instructionStack;
        
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
                //errs() << count++ << ": " << *it << "\n";
                //if (StoreInst *i = dyn_cast<StoreInst>(it)) {
                if (Instruction *i = dyn_cast<Instruction>(it)) {
                    /* if (isa<UnreachableInst>(i)) {
                     // Skip the unreachables...
                     continue;
                     } */
                    IRBuilder<> build();
                    errs() << "reversing " << *i << " instruction\n";
                    errs() << "It uses: ";
                    for (User::op_iterator u = i->op_begin(), e = i->op_end(); u != e; ++u) {
                        if (Instruction *v = dyn_cast<Instruction>(u)) {
                            errs() << v->getName() << " ";
                        }
                        else {
                            Value *v = dyn_cast<Value>(u);
                            errs() << "v_" << v->getName() << " ";
                        }
                    }
                    errs() << "\n";
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
            
            ValueToValueMapTy vmap;
            SmallVectorImpl<ReturnInst*> Returns(1);
            CloneFunctionInto(reverse, M.getFunction(OverFunc), vmap, true, Returns, "_reverse");
            
            reverse->viewCFG();
            
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
            
            if (builder) {
                delete builder;
                builder = 0;
            }
            builder = new IRBuilder<>(newBB);
            
            if (count == 0) {
                // No predecessors means this must be have been the entry.  Now it's the exit
                
                //
                reverseBlock(oldBB);
                //
                
                errs() << "count is zero\n";
                builder->CreateRetVoid();
            }
            
            if (count == 1) {
                // Straight-through
                
                //
                reverseBlock(oldBB);
                //
                
                errs() << "count is one\n";
                BasicBlock *key = pred_list[0];
                errs() << "Our key is " << *key << "\n";
                BasicBlock *data = oldToNew[key];
                errs() << "Our data is " << *data << "\n";
                builder->CreateBr(data);
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
        
        inline void transmute(Instruction *I) {
            errs() << "transmute: I'll try and figure out how to reverse " << *I << "\n";
            Value *v1 = I->getOperand(0);
            errs() << "operand(0) is " << *v1 << "\n";
            Value *v2 = I->getOperand(1);
            errs() << "operand(1) is " << *v2 << "\n";
            
            Instruction *r = instructionStack.back();
            //instructionStack.pop_back();
            errs() << "transmute: The top of the stack holds " << *r << "\n";
            
            Instruction *ret = 0;
            
            if (BinaryOperator *b = dyn_cast<BinaryOperator>(r)) {
                errs() << "OK we have a binary operator\n";
                
                llvm::Instruction::BinaryOps op = b->getOpcode();
                
                
                // See llvm/include/Instruction.def for a list of these constants
                switch (op) {
                    case Instruction::Add:
                        errs() << "Inserting sub in place of add\n";
                        v1 = b->getOperand(0);
                        errs() << "operand(0) is " << *v1 << "\n";
                        v2 = b->getOperand(1);
                        errs() << "operand(1) is " << *v2 << "\n";
                        ret = BinaryOperator::Create(Instruction::Sub, v1, v2, "r_add");
                        break;
                        
                    case Instruction::Sub:
                        errs() << "Inseting add in place of sub\n";
                        v1 = b->getOperand(0);
                        errs() << "operand(0) is " << *v1 << "\n";
                        v2 = b->getOperand(1);
                        errs() << "operand(1) is " << *v2 << "\n";
                        ret = BinaryOperator::Create(Instruction::Add, v1, v2, "r_sub");
                        break;
                        
                    case Instruction::Mul:
                        errs() << "Inserting udiv in place of mul\n";
                        v1 = b->getOperand(0);
                        errs() << "operand(0) is " << *v1 << "\n";
                        v2 = b->getOperand(1);
                        errs() << "operand(1) is " << *v2 << "\n";
                        ret = BinaryOperator::Create(Instruction::UDiv, v1, v2, "r_mul");
                        break;
                        
                        
                    default:
                        break;
                }
            }
            
            if (ret) {
                //Instruction * ret = BinaryOperator::Create(Instruction::Mul, v1, v2, "newInst");
                instructionStack.pop_back();
                
                errs() << "ret 1 is " << *ret << "\n";
                
                ReplaceInstWithInst(I, ret);
                
                errs() << "ret 2 is " << *ret << "\n";
            }
            else {
                errs() << "No appropriate operands to ReplaceInstWithInst\n";
            }
                        
            return;
        }
        
        
        virtual bool runOnModule(Module &M) {
            if (Function *rev = M.getFunction(OverFunc)) {
                DEBUG(errs() << "Found " << OverFunc << "\n");
                
                Function *target = createReverseFunction(M);
                
                // NEW STUFF HERE
                for (Function::iterator i = target->begin(), e = target->end(); i != e; ++i) {
                    // Print out the name of the basic block if it has one, and then the
                    // number of instructions that it contains
                    errs() << "Basic block (name=" << i->getName() << ") has "
                    << i->size() << " instructions.\n";
                    
                    instructionStack.clear();
                    
                    for (BasicBlock::iterator j = i->begin(), f = i->end(); j != f; ++j) {
                        // The next statement works since operator<<(ostream&,...)
                        // is overloaded for Instruction&
                        errs() << *j << "\n";
                        
                        if (isa<LoadInst>(j) || isa<StoreInst>(j)) {
                            errs() << "bypassing load / store\n";
                            errs() << *j << "\n\n";
                        }
                        else if (isa<TerminatorInst>(j)) {
                            errs() << "bypassing terminator\n";
                        }
                        else {
                            errs() << "We need to reverse " << *j << "\n\n";
                            instructionStack.push_back(j);
                        }
                    }
                    
                    
                    /*
                     Just brain storming here in comments...  So I think what's happening is that
                     we are swapping instructions, and some of those instructions were in the stack
                     and so now are deleted, which is a problem...  We may need to go from the front()
                     of the stack and the end() iterator to replace them...
                     OK so things are still getting squashed...  Create a map between every (read-only)
                     value from the original code and its (volatile) equivalent and look them up
                     */
                    
                    
                    BasicBlock::iterator j = i->begin();
                    
                    while (j != i->end()) {
                        Instruction *J = dyn_cast<Instruction>(j);
                    
                        errs() << "BasicBlock::iterator: " << *j << "\n";
                        
                        if (isa<LoadInst>(J) || isa<StoreInst>(J)) {
                            //errs() << "bypassing load / store\n";
                            //errs() << *j << "\n\n";
                        }
                        else if (isa<TerminatorInst>(J)) {
                            // Nothing!
                        }
                        else {
                            errs() << "instructionStack.size() is " << instructionStack.size() << "\n";
                            //Instruction *r = instructionStack.back();
                            //instructionStack.pop_back();
                            //errs() << "swapping " << *J << "with " << *r << "\n\n";
                            
                            // We have to do this here so we don't lose a valid iterator
                            ++j;
                            
                            transmute(J);
                            
                            //errs() << "transmute returned " << *t << "\n";
                            
                            target->viewCFG();
                                                        
                            continue;
                        }
                        ++j;
                    }
                }
                
                target->viewCFG();
                
                return true;
                
                // NEW STUFF HERE
                
                
                // Find the exit block from OverFunc
                BasicBlock *oldBB = findExitBlock(*rev);
                //BasicBlock *newEntryBB = BasicBlock::Create(M.getContext(), "r_" + oldBB->getName(), target);
                ValueToValueMapTy VMap;
                BasicBlock *newEntryBB = CloneBasicBlock(oldBB, VMap, "", target);
                newToOld.insert(std::make_pair(newEntryBB, oldBB));
                
                // Create all the new (empty) BBs
                for (Function::iterator i = rev->begin(), e = rev->end(); i != e; ++i) {
                    BasicBlock *bb = i;
                    if (bb == oldBB) {
                        continue;
                    }
                    errs() << "BB info: " << i->getName() << "\n";
                    //BasicBlock *newBB = BasicBlock::Create(M.getContext(), "r_" + i->getName(), target);
                    ValueToValueMapTy VMap;
                    BasicBlock *newBB = CloneBasicBlock(bb, VMap, "", target);
                    errs() << "Reverse BB info: " << newBB->getName() << "\n";
                    newToOld.insert(std::make_pair(newBB, bb));
                    oldToNew.insert(std::make_pair(bb, newBB));
                }
                
                //target->viewCFG();
                
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

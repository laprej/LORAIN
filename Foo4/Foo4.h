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
#include "llvm/IRBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CFG.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ValueSymbolTable.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SCCIterator.h"

#include "llvm/Assembly/Writer.h"

#include <stack>
#include <set>
#include <list>

#include "llvm/Analysis/MemoryDependenceAnalysis.h"

using namespace llvm;



namespace {
    
#pragma mark
#pragma mark Hello
    
    class Inverter;
    
    // Hello - The first implementation, without getAnalysisUsage.
    class Hello : public ModulePass {
    public:
        static char ID; // Pass identification, replacement for typeid
        Hello() : ModulePass(ID)
        {
            builder = 0;
        }

        /// Our Inverter class
        Inverter *inv;
        
        std::list<Instruction*> instructionStack;
        std::list<Instruction*> reverseInstructionStack;
		
		/// BBM = BasicBlock Map
        std::map<BasicBlock *, BasicBlock *> bbmNewToOld;
        //std::map<BasicBlock *, BasicBlock *> bbmOldToNew;
        
        std::map<const Value *, Value *> duals;
        
        IRBuilder<> *builder;
        
        LoopInfo * getLoopInfo(Function &f);
        
        BasicBlock * findDom(BasicBlock *bb);
        
        MDNode * domTreeLookup(Instruction &I);
        
        BasicBlock * postdomTreeLookup(BasicBlock *bb);
        
        BasicBlock *reverseBlock(BasicBlock *B);
        
        Function *createReverseFunction(Module &M);
        
        
        /**
         This function assumes that the IR is single-exit so we have to
         run that pass first
         */
        BasicBlock *findExitBlock(Function &F);
        
        /// Change all Values from ``from'' to ``to''
        void updateMap(Value *from, Value *to);		
		void outputMap();		
		Value * lookup(Value *k);		
		/// Change all Values from ``from'' to ``to''
        void updateList(Instruction *from, Instruction *to);		
		/// Stack dumper
		void outputList(std::list<Instruction*> l, std::string s);
        
		void eraseBlockContents(BasicBlock *bb);
		
		/// Negate a binary operator and all of its deps.
		/// Use a builder to handle it.
		void negateStoreInstruction(StoreInst *b);
        
		virtual void getAnalysisUsage(AnalysisUsage &Info) const;
		
        virtual bool runOnModule(Module &M);
    };
}

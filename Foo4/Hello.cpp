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
#include "Foo4.h"
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

#include "llvm/Analysis/MemoryDependenceAnalysis.h"

using namespace llvm;



namespace {
    static cl::opt<std::string> OverFunc("rev-func",
										 cl::desc("<func to reverse>"));//, llvm::cl::Required);	
	
	static cl::opt<std::string> TargetFunc("tgt-func",
										   cl::desc("<func to output>"));//, llvm::cl::Required);
	
	std::map<BasicBlock *, BasicBlock *> bbmOldToNew;
	
#pragma mark
#pragma mark Instrumenter
    
	/// This class will apply the necessary instrumentation on the forward path
	class Instrumenter : public InstVisitor<Instrumenter>
	{
		Module &M;
        
        unsigned bitFieldCount;// = 0;
        
	public:
		Instrumenter(Module &mod): M(mod) {
            bitFieldCount = 0;
		}
		
		/* /// GetOrCreateAnchor - Look up an anchor for the specified tag and name.  If it
         /// already exists, return it.  If not, create a new one and return it.
         DIAnchor DIFactory::GetOrCreateAnchor(unsigned TAG, const char *Name) {
         const Type *EltTy = StructType::get(Type::Int32Ty, Type::Int32Ty, NULL);
         
         // Otherwise, create the global or return it if already in the module.
         Constant *C = M.getOrInsertGlobal(Name, EltTy);
         assert(isa<GlobalVariable>(C) && "Incorrectly typed anchor?");
         GlobalVariable *GV = cast<GlobalVariable>(C);
         
         // If it has an initializer, it is already in the module.
         if (GV->hasInitializer()) 
         return SubProgramAnchor = DIAnchor(GV);
         
         GV->setLinkage(GlobalValue::LinkOnceAnyLinkage);
         GV->setSection("llvm.metadata");
         GV->setConstant(true);
         M.addTypeName("llvm.dbg.anchor.type", EltTy);
         
         // Otherwise, set the initializer.
         Constant *Elts[] = {
         GetTagConstant(dwarf::DW_TAG_anchor),
         ConstantInt::get(Type::Int32Ty, TAG)
         };
         
         GV->setInitializer(ConstantStruct::get(Elts, 2));
         return DIAnchor(GV);
         } */
		
        std::string newBitFieldName(StringRef s)
        {
            //static unsigned bitFieldCount = 0;
            //
            Twine bff(s);
            
            bff = bff.concat(Twine(bitFieldCount++));
            
            errs() << "newBitFieldName is returning " << bff << "\n";
            
            return bff.str();
        }
        
		Constant * insertBitField(StringRef Name) {
			Type *Ty = IntegerType::get(getGlobalContext(), 32);
			
			Constant *C = M.getOrInsertGlobal(Name, Ty);
			assert(isa<GlobalVariable>(C) && "Incorrectly typed anchor?");
			GlobalVariable *GV = cast<GlobalVariable>(C);
			
			// If it has an initializer, it is already in the module.
			if (GV->hasInitializer()) {
				assert(0 && "Re-creating existing variable!");
			}
			
			GV->setLinkage(GlobalValue::LinkOnceAnyLinkage);
			//GV->setSection("llvm.metadata");
			GV->setConstant(false);
			//M.addTypeName("llvm.dbg.anchor.type", EltTy);
			
			// Otherwise, set the initializer.
			//Constant *Elts[] = {
			//	GetTagConstant(dwarf::DW_TAG_anchor),
			//	ConstantInt::get(Type::Int32Ty, TAG)
			//};
			//
			//GV->setInitializer(ConstantStruct::get(Elts, 2));
			
			IRBuilder<> temp(getGlobalContext());
			GV->setInitializer(temp.getInt32(0));
			
			return C;
		}
		
		void markJML(Value *v) {
			if (Instruction *I = dyn_cast<Instruction>(v)) {
				MDNode *Node = MDNode::get(getGlobalContext(), 0);
				//NamedMDNode *NMD = M.getOrInsertNamedMetadata("jml.new.var");
				//NMD->addOperand(Node);
				I->setMetadata("jml", Node);
			}
			
			if (CmpInst *I = dyn_cast<CmpInst>(v)) {
				/// Assert that the next instruction is a branch
				BasicBlock::iterator it(I);
				++it;
				if (BranchInst *B = dyn_cast<BranchInst>(it)) {
					assert(B->getNumSuccessors() == 2 && "Branch doesn't have two sucessors!");
					BasicBlock *then = B->getSuccessor(0);
					BasicBlock *el = B->getSuccessor(1);
					Value *Elts[] = {
						MDString::get(getGlobalContext(), then->getName()),
						MDString::get(getGlobalContext(), el->getName()),
                        ConstantInt::get(Type::getInt32Ty(getGlobalContext()), bitFieldCount)
                        //                        MDString::get(getGlobalContext(), StringRef(bitFieldCount))
					};
					MDNode *Node = MDNode::get(getGlobalContext(), Elts);
					NamedMDNode *NMD = M.getOrInsertNamedMetadata("jml.icmp");
					NMD->addOperand(Node);
					I->setMetadata("jml.icmp", Node);
				}
				else {
					assert(isa<BranchInst>(it) && "CmpInst not followed by BranchInst!");
				}
			}
		}
		
		void visitCmpInst(CmpInst &I) {
			/*
             * What we need to do here is emit a new global variable and assign
             * it the result of the cmp instruction in the function to be
             * reversed.
             */
            DEBUG(errs() << "COMPARE INSTRUCTION\n");
			
#if 0
			{
				if (I.getMetadata("jml")) {
					return;
				}
				
				/* Instruction *pi = ...;
				 Instruction *newInst = new Instruction(...);
				 
				 pi->getParent()->getInstList().insert(pi, newInst); */
				
				Instruction *pi = &I;
				Instruction *newInst = I.clone();
				
				/*
				 This is from http://llvm.org/docs/SourceLevelDebugging.html
				 
				 if (MDNode *N = I->getMetadata("dbg")) {  // Here I is an LLVM instruction
				 DILocation Loc(N);                      // DILocation is in DebugInfo.h
				 unsigned Line = Loc.getLineNumber();
				 StringRef File = Loc.getFilename();
				 StringRef Dir = Loc.getDirectory();
				 }
				 */
				
				/// See DIBuilder.cpp for more examples
				Value *Elts[] = {
					//MDString::get(getGlobalContext(), "jml.new.var"),
					ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0),
					ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0),
					llvm::Constant::getNullValue(Type::getInt32Ty(getGlobalContext())),
					ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0),
					llvm::Constant::getNullValue(Type::getInt32Ty(getGlobalContext())),
				};
				
				MDNode *Node = MDNode::get(getGlobalContext(), 0);
				//NamedMDNode *NMD = M.getOrInsertNamedMetadata("jml.new.var");
				//NMD->addOperand(Node);
				
				newInst->setMetadata("jml", Node);
				
				if (newInst->hasMetadata()) {
					errs() << "has metadata\n";
				}
				else {
					errs() << "does not have metadata\n";
				}
				if (MDNode *N = newInst->getMetadata("jml")) {
					errs() << "We found JML\n";
				}
				pi->getParent()->getInstList().insertAfter(pi, newInst);
			}
#endif
			
            // We are creating a dangerous coupling here between our metadata
            // necessary for marking a cmp instruction and our bitfield count.
            // We'll have to revisit this if it becomes a problem
			markJML(&I);
			
			BasicBlock::iterator it(I);
			++it;
			IRBuilder<> b(it);
			
			/// Manually emit the load, add, store for this
			/// Don't forget to tag all this JML!
			//const Type *newGlobal = IntegerType::get(I.getContext(), 32);
			//Value *l = M.getOrInsertGlobal("bf", newGlobal);
            
            StringRef bf = newBitFieldName("bf");
			
			Value *l = insertBitField(bf);
			markJML(l);
			
			Value *ll = b.CreateLoad(l);
			markJML(ll);
			
			errs() << "ll has type: " << *ll->getType() << "\n";
			errs() << "I has type: " << *I.getType() << "\n";
			
			Value *lll = b.CreateCast(Instruction::Trunc, ll, I.getType());
			markJML(lll);
			
			errs() << "lll has type: " << *lll->getType() << "\n";
			
			Value *v = b.CreateNUWAdd(lll, &I);
			markJML(v);
			
			errs() << "got here...\n";
			
			Value *llll = b.CreateCast(Instruction::SExt, v, ll->getType());
			markJML(llll);
			
			errs() << "llll has type: " << *llll->getType() << "\n";
			
			markJML(b.CreateStore(llll, l));
			
			
			errs() << "Here, too!\n";
		}
	};
    
    class Hello;
    
#pragma mark
#pragma mark Inverter
    
    class Inverter : public InstVisitor<Inverter>
    {
		bool currently_reversing;
		std::map<Value *, Value *> oldToNew;
		IRBuilder<> &builder;
		Value *lastVal;
		Module &M;
        Hello *h;
		
    public:
        
		Inverter(IRBuilder<> &b, Module &mod, Hello *H): builder(b), M(mod), h(H) { 
			currently_reversing = true;
			lastVal = 0;
		}
		
		Value * lookup(Value *k) {
			errs() << "\n\n\nLOOKUP\n";
			std::map<Value *, Value *>::iterator it;
			
			it = oldToNew.find(k);
			if (it == oldToNew.end()) {
				errs() << *k << " not found, outputting map\n";
				outputMap();
				return k;
			}
			
			return it->second;
		}
		
		void outputMap() {
			std::map<Value *, Value *>::iterator it, e;
			
			errs() << "MAP BEGIN\n";
			for (it = oldToNew.begin(), e = oldToNew.end(); it != e; ++it) {
				errs() << "key: " << *it->first << "\n\tdata: " << *it->second << "\n";
			}
			errs() << "MAP END\n";
		}
        
        MDNode * findDiamondBf(Instruction &I)
        {
            MDNode * md = h->domTreeLookup(I);
            
            return md;
        }
		
		void visitTerminatorInst(TerminatorInst &I) {
			errs() << "\n\n\nTERMINATOR INSTRUCTION\n";
			
			/* if (isa<ReturnInst>(I)) {
             builder.CreateRetVoid();
             }
             if (isa<BranchInst>(I)) {
             //builder.CreateBr();
             // OK so here we're using a hack that will not work when there are more than
             // two basic blocks.  Find the function the builder is a part of.  Get the
             // list of BBs.  Whichever one is not the current block must be the target
             BasicBlock *bb = builder.GetInsertBlock();
             Function *f = bb->getParent();
             for (Function::iterator it = f->begin(), e = f->end(); it != e; ++it) {
             if (&*it != bb) {
             builder.CreateBr(it);
             }
             }
             } */
			
			int count = 0;
			
			BasicBlock *bb = I.getParent();
            
            for (pred_iterator PI = pred_begin(bb), E = pred_end(bb); PI != E; ++PI) {
                BasicBlock *Pred = *PI;
                errs() << "pred of " << *bb << ": " << *Pred << "\n";
                // ...
                //pred_list.push_back(Pred);
                
                count++;
            }
			
			BasicBlock *foo = builder.GetInsertBlock();
			
			errs() << "count is " << count << "\n";
			
			/// If count is zero, it must be the entry block so make it exit
			if (count == 0) {
				builder.CreateRetVoid();
				return;
			}
			
			/// Count is == 1 -- find the pred
			if (count == 1) {
				BasicBlock *pred = *pred_begin(bb);
				BasicBlock *newSucc = bbmOldToNew[pred];
				builder.CreateBr(newSucc);
				return;
			}
			
			if (count == 2) {
                
                //findDiamondBf(I);
                
				NamedMDNode *nmd = M.getNamedMetadata("jml.icmp");
				assert(nmd);
				errs() << "NMD: " << nmd->getName();// << "\n";
				errs() << " has " << nmd->getNumOperands() << " operands\n";
				//MDNode *md = nmd->getOperand(0);
                
                MDNode *md = findDiamondBf(I);
				
                errs() << md->getName() << " has " << md->getNumOperands() << " operands\n";
				MDString *then = dyn_cast_or_null<MDString>(md->getOperand(0));
				assert(then);
				MDString *el   = dyn_cast_or_null<MDString>(md->getOperand(1));
				assert(el);
                ConstantInt *bfn = dyn_cast_or_null<ConstantInt>(md->getOperand(2));
                assert(bfn);
				/// We have two predecessors; we're going to need an "if"
				errs() << "WE NEED A BRANCH\n";
				/// Emit a load of bf, compare, and jump to appropriate BBs
				
				/// CURRENTLY WORKING ON THIS
				/// Manually emit the load & cmp the bf
				/// Don't forget to tag all this JML!
                Type *newGlobal = IntegerType::get(I.getContext(), 32);
                Twine bfX("bf");
                bfX = bfX.concat(Twine(bfn->getZExtValue()));
                errs() << "bfn->getZExtValue() returned " << bfn->getZExtValue() << "\n";
                errs() << "and bfX is " << bfX << "\n";
				Value *l = M.getOrInsertGlobal(bfX.str(), newGlobal);
				//markJML(l);
				
				Value *ll = builder.CreateLoad(l);
				//markJML(ll);
				
				errs() << "ll has type: " << *ll->getType() << "\n";
				errs() << "I has type: " << *I.getType() << "\n";
				
				Value *lll = builder.CreateCast(Instruction::Trunc, ll, IntegerType::get(I.getContext(), 1));
				//markJML(lll);
				
				errs() << "lll has type: " << *lll->getType() << "\n";
				
				Value *llll = builder.CreateICmpEQ(lll, builder.getInt1(true));
                
                /// Reset the bitfield
                Value *lllll = builder.CreateSub(ll, ll);
                /// Store back the bitfield
                Value *llllll = builder.CreateStore(lllll, l);
				
				BasicBlock *thenBB = 0, *elBB = 0;
				Function *F = foo->getParent();
				/// Find the "then" block
				Function::iterator it, E;
				for (it = F->begin(), E = F->end(); it != E; ++it) {
					std::string str = then->getString();
					str += "_rev";
					errs() << "Comparing " << str << " and " << it->getName();
					if (str == it->getName()) {
						errs() << " Got it!\n";
						thenBB = it;
					}
					errs() << "\n";
					
					std::string str2 = el->getString();
					str2 += "_rev";
					errs() << "Comparing " << str2 << " and " << it->getName();
					if (str2 == it->getName()) {
						errs() << " Got it!\n";
						elBB = it;
					}
					errs() << "\n";
				}
				
				assert(thenBB);
				assert(elBB);
				/// Create a branch instruction here that jumps to the bb names
				/// stored in the metadata
				builder.CreateCondBr(llll, thenBB, elBB);
				
				/*
                 
                 Value *v = builder.CreateNUWAdd(lll, &I);
                 //markJML(v);
                 
                 errs() << "got here...\n";
                 
                 Value *llll = builder.CreateCast(Instruction::SExt, v, ll->getType());
                 //markJML(llll);
                 
                 errs() << "llll has type: " << llll->getType()->getDescription() << "\n";
                 
                 //markJML(b.CreateStore(llll, l));
                 
                 
                 errs() << "Here, too!\n";
                 
                 exit(-1);
                 */
				
			}
			/* if (BranchInst *b = dyn_cast<BranchInst>(&I)) {
             if (b->isUnconditional()) {
             BasicBlock *oldSucc = b->getSuccessor(0);
             BasicBlock *newSucc = static_cast<BasicBlock*>(oldToNew[oldSucc]);
             builder.CreateBr(newSucc);
             return;
             }
             else {
             assert(0 && "Conditional branches not yet supported!\n");
             }
             
             } */
			
			errs() << "I is a " << I << "\n";
			
			//assert(0 && "Unhandled terminator instruction!\n");
		}
		
		void visitAllocaInst(AllocaInst &I) {
			errs() << "\n\n\nALLOCA INSTRUCTION\n";
			
			Value *alloc = builder.CreateAlloca(I.getType(), 0, I.getName());
			errs() << "Allocating a " << *I.getType() << "\n";
			
			oldToNew[&I] = alloc;
		}
        
        void visitStoreInst(StoreInst &I) {
            errs() << "\n\n\nSTORE INSTRUCTION\n";
			
			std::vector<Value *> bucket;
			oldToNew.clear();
            
            Value *storeVal = I.getPointerOperand();
			if (isa<GlobalValue>(storeVal)) {
                errs() << storeVal->getName() << " is a global value\n";
				currently_reversing = true;
            }
            else {
                errs() << storeVal->getName() << " is not a global value\n";
				currently_reversing = false;
            }
			
			getUseDef(&I, bucket);
			
			errs() << I << "\n";
			errs() << "Bucket contains:\n";
			for (std::vector<Value *>::iterator it = bucket.begin(), e = bucket.end();
				 it != e; ++it) {
				errs() << **it << "\n";
			}
			
			while (bucket.size()) {
				Value *v = bucket.back();
				bucket.pop_back();
				
				if (Instruction *i = dyn_cast<Instruction>(v)) {
					visit(i);
				}
			}
			
			assert(lastVal && "lastVal not set!");
			
			builder.CreateStore(lastVal, storeVal);
			
			lastVal = 0;
		}
		
		void visitLoadInst(LoadInst &I) {
			errs() << "\n\n\nLOAD INSTRUCTION\n";
			
			Value *valOfI = &I;
			
			Value *loadVal = I.getPointerOperand();
			if (isa<GlobalValue>(loadVal)) {
				errs() << loadVal->getName() << " is a global value\n";
			}
			else {
				errs() << loadVal->getName() << " is not a global value\n";
			}
			
			/// Create a new load
			LoadInst *inst = builder.CreateLoad(loadVal);
			
			oldToNew[valOfI] = inst;
			outputMap();
			errs() << "\n\n\nLOAD INSTRUCTION END\n";
		}
		
		void visitBinaryOperator(BinaryOperator &I) {
			Value *newInstruction;
			DEBUG(errs() << "\n\n\nBINARY OPERATOR\n");
            
			if (I.getOpcode() == Instruction::Add) {
				DEBUG(errs() << "ADD INSTRUCTION\n");
                //DEBUG(errs() << "lastVal: " << *lastVal << "\n");
                DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
				//lastVal = builder.CreateSub(lastVal, I.getOperand(1));
				newInstruction = builder.CreateSub(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
			}
			
			if (I.getOpcode() == Instruction::Sub) {
				DEBUG(errs() << "SUB INSTRUCTION\n");
                //DEBUG(errs() << "lastVal: " << *lastVal << "\n");
                DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
				//lastVal = builder.CreateAdd(lastVal, I.getOperand(1));
				newInstruction = builder.CreateAdd(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
			}
			
			if (I.getOpcode() == Instruction::Mul) {
				DEBUG(errs() << "MULT INSTRUCTION\n");
                //DEBUG(errs() << "lastVal: " << *lastVal << "\n");
                DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
				//lastVal = builder.CreateSDiv(lastVal, I.getOperand(1));
                //lastVal = builder.CreateSDiv(I.getOperand(0), I.getOperand(1));
				newInstruction = builder.CreateSDiv(lookup(I.getOperand(0)), lookup(I.getOperand(1)));
			}
			
			oldToNew[&I] = newInstruction;
			lastVal = newInstruction;
		}
		
		/// Collect all use-defs into a container
		void getUseDef(User *I, std::vector<Value *> &bucket, int indent = 0) {
			if (indent == 0) {
				DEBUG(errs() << "uses\n");
			}
			
            int uses = 0;
            
			for (User::op_iterator i = I->op_begin(), e = I->op_end();
				 i != e; ++i) {
				uses++;
                
				Value *v = *i;
				
				if (Instruction *w = dyn_cast<Instruction>(v)) {
					DEBUG(errs() << std::string(2*indent, ' '));
					DEBUG(errs() << "outputing instruction dependent: " << *w << '\n');
					
					/// Don't store alloca's if that's a dependent!!!
					if (!isa<AllocaInst>(w)) {
						bucket.push_back(w);
					}
					//bucket.push_back(w);
					getUseDef(w, bucket, indent + 1);
					DEBUG(errs() << std::string(2*indent, ' '));
                    DEBUG(errs() << "bucket now has " << bucket.size() << " elements\n");
				}
                else {
					DEBUG(errs() << std::string(2*indent, ' '));
                    DEBUG(errs() << "This is a " << *v->getType() << "\n");
                }
				
				
				/*
				 else if (Operator *w = dyn_cast<Operator>(v)) {
				 getUseDef(w, indent + 1);
				 } else if (Constant *w = dyn_cast<Constant>(v)) {
				 getUseDef(w, indent + 1);
				 }
				 */
			}
			DEBUG(errs() << std::string(2*indent, ' '));
            DEBUG(errs() << "This instruction has " << uses << " uses\n\n");
		}
    };
    
    class Swapper {
        
    };
    
#pragma mark
#pragma mark Hello
    
    //    // Hello - The first implementation, without getAnalysisUsage.
    //    class Hello : public ModulePass {
    //    public:
    //        static char ID; // Pass identification, replacement for typeid
    //        Hello() : ModulePass(ID) {
    //            builder = 0;
    //        }
    //        
    //        /// Our Inverter class
    //        Inverter *inv;
    //        
    //        std::list<Instruction*> instructionStack;
    //        std::list<Instruction*> reverseInstructionStack;
    //		
    //		/// BBM = BasicBlock Map
    //        std::map<BasicBlock *, BasicBlock *> bbmNewToOld;
    //        //std::map<BasicBlock *, BasicBlock *> bbmOldToNew;
    //        
    //        std::map<const Value *, Value *> duals;
    //        
    //        IRBuilder<> *builder;
    
    
    /// Lookup our IDom (from a merge point) so we can find the correct
    /// metadata to tell us where to go (BBs) and what to check (bitfields)
    MDNode * Hello::domTreeLookup(Instruction &I)
    {
        Function *f = I.getParent()->getParent();
        DominatorTree &DT = getAnalysis<DominatorTree>(*f);
        DomTreeNode *Rung = DT.getNode(I.getParent());
        Rung = Rung->getIDom();
        errs() << "Rung is " << Rung << "\n";
        BasicBlock *bb = Rung->getBlock();
        errs() << "BB is " << *bb << "\n";
        
        CmpInst *C = 0;
        
        // find the cmp
        for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
            if (C = dyn_cast<CmpInst>(i)) {
                errs() << *C << "\n";
                break;
            }
        }
        
        assert(C && "CmpInst not found!");
        
        MDNode *md = C->getMetadata("jml.icmp");
        
        assert(md && "No metadata found on CmpInst!");
        
        return md;
    }
    
    BasicBlock *Hello::reverseBlock(BasicBlock *B)
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
                        Value *w = dyn_cast<Value>(u);
                        errs() << "w_" << w->getName() << " ";
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
        return 0;
    }
    
    Function *Hello::createReverseFunction(Module &M)
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
        
        //reverse->viewCFG();
        
        return reverse;
    }
    
    
    /**
     This function assumes that the IR is single-exit so we have to
     run that pass first
     */
    BasicBlock *Hello::findExitBlock(Function &F)
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
    
    void Hello::reverseBlockInto(BasicBlock *oldBB, BasicBlock *newBB)
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
            BasicBlock *data = bbmOldToNew[key];
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
    
    /// Change all Values from ``from'' to ``to''
    void Hello::updateMap(Value *from, Value *to) {
        
        errs() << "We are swapping\n\t";
        errs() << *from << " out for\n\t";
        errs() << *to << "\n";
        
        std::map<const Value *, Value *>::iterator it, e;
        
        for (it = duals.begin(), e = duals.end(); it != e; ++it) {
            const Value *key = it->first;
            Value *data = it->second;
            
            if (data == from) {
                duals.erase(it);
                duals.insert(std::make_pair(key, to));
            }
            
            if (key == from) {
                duals.erase(it);
                duals.insert(std::make_pair(to, data));
            }
        }
    }
    
    void Hello::outputMap() {
        std::map<const Value *, Value *>::iterator it, e;
        
        errs() << "MAP BEGIN\n";
        for (it = duals.begin(), e = duals.end(); it != e; ++it) {
            errs() << "key: " << *it->first << "\n\tdata: " << *it->second << "\n";
        }
        errs() << "MAP END\n";
    }
    
    Value * Hello::lookup(Value *k) {
        errs() << "LOOKUP\n";
        std::map<const Value *, Value *>::iterator it;
        
        it = duals.find(k);
        if (it == duals.end()) {
            errs() << *k << " not found, outputting map\n";
            outputMap();
            return k;
        }
        
        return it->second;
    }
    
    /// Change all Values from ``from'' to ``to''
    void Hello::updateList(Instruction *from, Instruction *to) {
        //std::list<Instruction*> instructionStack;
        std::list<Instruction *>::iterator it, e;
        
        errs() << "swapping " << *from << " to " << *to << "\n";
        
        for (it = instructionStack.begin(), e = instructionStack.end(); it != e; ++it) {
            Instruction *data = *it;
            
            if (data == from) {
                instructionStack.insert(it, to);
                instructionStack.erase(it);
                //duals.erase(it);
                //duals.insert(std::make_pair(key, to));
            }
        }
    }
    
    /// Stack dumper
    void Hello::outputList(std::list<Instruction*> l, std::string s) {
        errs() << s + "\n";
        std::list<Instruction *>::iterator it, e;
        for (it = l.begin(), e = l.end(); it != e; ++it) {
            Instruction *inst = *it;
            errs() << *inst << "\n";
        }
        errs() << s + " END\n";
    }
    
    
    /**
     OK, this function does not currently do this, but it should in the future
     do the following:
     
     Two Code Paths:
     A, the original block we intend to reverse, and
     B, the reversed counterpart of A
     
     The instructionStack contains instructions from A pushed onto the stack
     in the order in which they are seen, e.g. they top of the stack has the
     last instruction.  What we need to do is:
     
     1. Pop the instruction off the stack
     2. Figure out its inverse
     3. Look up the corresponding instruction in B
     4. Replace 3 with 2
     */
    inline void Hello::transmute(/*Instruction *I*/) {
        outputList(instructionStack, "INSTRUCTION STACK");
        outputList(reverseInstructionStack, "REVERSE INSTRUCTION STACK");
        
        Instruction *I = instructionStack.back();
        instructionStack.pop_back();
        
        Instruction *RI = reverseInstructionStack.front();
        reverseInstructionStack.pop_front();
        
        errs() << "transmute: I'll try and figure out how to reverse " << *I << "\n";
        Value *v1 = I->getOperand(0);
        errs() << "operand(0) is " << *v1 << "\n";
        Value *v2 = I->getOperand(1);
        errs() << "operand(1) is " << *v2 << "\n";
        
        errs() << "transmute: The stack size is " << instructionStack.size() << "\n";
        //Instruction *r = instructionStack.back();
        Instruction *r = I;
        outputList(instructionStack, "INSTRUCTION STACK");
        //instructionStack.pop_back();
        errs() << "transmute: The top of the stack holds " << *r << "\n";
        
        Instruction *ret = 0;
        
        if (BinaryOperator *b = dyn_cast<BinaryOperator>(r)) {
            errs() << "OK we have a binary operator\n";
            
            llvm::Instruction::BinaryOps op = b->getOpcode();
            
            // Fix the operands, they're still in original stack!
            
            
            // See llvm/include/Instruction.def for a list of these constants
            switch (op) {
                case Instruction::Add:
                    errs() << "Inserting sub in place of add\n";
                    v1 = b->getOperand(0);
                    errs() << "operand(0) is " << *v1 << "\n";
                    v2 = b->getOperand(1);
                    errs() << "operand(1) is " << *v2 << "\n";
                    ret = BinaryOperator::Create(Instruction::Sub, lookup(v1), lookup(v2), "r_add");
                    break;
                    
                case Instruction::Sub:
                    errs() << "Inseting add in place of sub\n";
                    v1 = b->getOperand(0);
                    errs() << "operand(0) is " << *v1 << "\n";
                    v2 = b->getOperand(1);
                    errs() << "operand(1) is " << *v2 << "\n";
                    ret = BinaryOperator::Create(Instruction::Add, lookup(v1), lookup(v2), "r_sub");
                    break;
                    
                case Instruction::Mul:
                    errs() << "Inserting udiv in place of mul\n";
                    v1 = b->getOperand(0);
                    errs() << "operand(0) is " << *v1 << "\n";
                    v2 = b->getOperand(1);
                    errs() << "operand(1) is " << *v2 << "\n";
                    ret = BinaryOperator::Create(Instruction::UDiv, lookup(v1), lookup(v2), "r_mul");
                    break;
                    
                    
                default:
                    break;
            }
        }
        
        if (ret) {
            outputMap();
            std::map<const Value *, Value *>::iterator mapit;
            mapit = duals.find(RI);
            if (mapit == duals.end()) {
                errs() << *RI << " could not be found in duals!\n";
                exit(-1);
            }
            errs() << "Here1\n";
            Value * val = (*mapit).second;
            errs() << "Here2\n";
            errs() << "val is " << *val << "\n";
            
            Instruction *newInst = dyn_cast<Instruction>(val);
            errs() << "Here3\n";
            
            //Instruction * ret = BinaryOperator::Create(Instruction::Mul, v1, v2, "newInst");
            errs() << "transmute2: The stack size is " << instructionStack.size() << "\n";
            //instructionStack.pop_back();
            errs() << "transmute2: The stack size is " << instructionStack.size() << "\n";
            
            errs() << "ret 1 is " << *ret << "\n";
            
            outputList(instructionStack, "INSTRUCTION STACK");
            
            
            // Insertion into an explicit instruction list
            // http://llvm.org/docs/ProgrammersManual.html#schanges_creating
            //BasicBlock *pb = newInst->getParent();
            //pb->getInstList().insert(newInst, ret); // Inserts ret before newInst in pb
            
            ReplaceInstWithInst(newInst, ret);
            //ReplaceInstWithInst(I, ret);
            
            //BasicBlock::iterator ii(newInst);
            //ReplaceInstWithValue(newInst->getParent()->getInstList(), ii, val);
            
            //updateMap(I, ret);
            updateMap(newInst, ret);
            //updateList(newInst, ret);
            outputList(instructionStack, "INSTRUCTION STACK");
            
            errs() << "ret 2 is " << *ret << "\n";
            
            //instructionStack.pop_back();
            outputList(instructionStack, "INSTRUCTION STACK");
        }
        else {
            errs() << "No appropriate operands to ReplaceInstWithInst\n";
            exit(-1);
        }
        
        return;
    }
    
    void Hello::eraseBlockContents(BasicBlock *bb) {
        BasicBlock::iterator bbi, bbe;
        
        errs() << "Erasing contents of basic block " << bb->getName() << "\n";
        
        std::vector<Instruction*> ivec;
        
        for (bbi = bb->begin(), bbe = bb->end(); bbi != bbe; ++bbi) {
            if (!isa<TerminatorInst>(bbi)) {
                //
                ivec.push_back(bbi);
            }
        }
        
        while (ivec.size()) {
            Instruction *i = ivec.back();
            ivec.pop_back();
            i->eraseFromParent();
        }
    }
    
    /// Negate a binary operator and all of its deps.
    /// Use a builder to handle it.
    void Hello::negateStoreInstruction(StoreInst *b) {
        std::vector<Value *> bucket;
        //getUseDef(b, bucket);
        
        errs() << "\n\n\nAfter negateStoreInstruction bucket holds:\n";
        std::vector<Value *>::iterator it = bucket.end(), begin = bucket.begin();
        --it;
        
        int num = 0;
        
        std::vector<Value *>::reverse_iterator rit(bucket.end()), re(bucket.begin());
        for (; rit != re; ++rit) {
            errs() << num++ << ": " << "(" << b->getParent()->getName() << ") " << **rit << "\n";
        }
        
    }
    
    void Hello::getAnalysisUsage(AnalysisUsage &Info) const
    {
        //Info.addRequired<AliasAnalysis>();
        Info.addRequired<MemoryDependenceAnalysis>();
        Info.addRequiredTransitive<DominatorTree>();
        //Info.addRequiredTransitive<MemoryDependenceAnalysis>();
    }
    
    //virtual bool
    
    bool Hello::runOnModule(Module &M) {
        if (Function *rev = M.getFunction(OverFunc)) {
            Instrumenter ins(M);
            for (inst_iterator I = inst_begin(rev), E = inst_end(rev); I != E; ++I)
                ins.visit(&*I);
            
            DEBUG(errs() << "Found " << OverFunc << "\n");
            
            Function *target = createReverseFunction(M);
            
            target->deleteBody();
            
            //std::map<BasicBlock*,BasicBlock*> oldToNew;
            //std::map<BasicBlock*,BasicBlock*> oldToNewRev;
            
            /// Put exit BB first in function
            BasicBlock *bb = findExitBlock(*rev);
            BasicBlock *newBlock = BasicBlock::Create(getGlobalContext(),
                                                      "exit_rev", target);
            bbmOldToNew[bb] = newBlock;
            bbmNewToOld[newBlock] = bb;
            
            /// Make analogs to all BBs in function
            Function::iterator fi, fe;
            for (fi = rev->begin(), fe = rev->end(); fi != fe; ++fi) {
                if (bb == fi) {
                    continue;
                }
                BasicBlock *block = BasicBlock::Create(getGlobalContext(),
                                                       fi->getName() +
                                                       "_rev", target);
                bbmOldToNew[fi] = block;
                bbmNewToOld[block] = fi;
            }
            
            DEBUG(errs() << "HERE\n");
            
            for (fi = rev->begin(), fe = rev->end(); fi != fe; ++fi) {
                /// Create our BasicBlock
                //BasicBlock *block = BasicBlock::Create(getGlobalContext(),
                //									   "", target);
                BasicBlock *block = bbmOldToNew[fi];
                
                IRBuilder<> builder(block);
                /// Create a new Inverter (one for each BB)
                Inverter inv(builder, M, this);
                
                std::stack<StoreInst*> stores;
                
                for (BasicBlock::iterator j = fi->begin(), k = fi->end(); j != k; ++j) {
                    if (j->getMetadata("jml")) {
                        continue;
                    }
                    
                    if (AllocaInst *a = dyn_cast<AllocaInst>(j)) {
                        inv.visitAllocaInst(*a);
                    }
                    
                    /*
                     if (TerminatorInst *t = dyn_cast<TerminatorInst>(j)) {
                     inv.visitTerminatorInst(*t);
                     }
                     */
                    
                    if (StoreInst *b = dyn_cast<StoreInst>(j)) {
                        //negateStoreInstruction(b);
                        //Inverter inv;
                        
                        //AliasAnalysis AA = getAnalysis<AliasAnalysis>();
                        
                        //errs() << "Got AA\n";
                        
                        MemDepResult mdr;
                        
                        MemoryDependenceAnalysis& mda = getAnalysis<MemoryDependenceAnalysis>(*rev);
                        
                        DEBUG(errs() << "Got MDR\n");
                        
                        mdr = mda.getDependency(b);
                        
                        DEBUG(errs() << "Instruction " << *b << ": ");
                        
                        DEBUG(errs() << "MDR: " << *mdr.getInst() << "\n");
                        
                        // Make sure local stores only store to local loads
                        if (!isa<GlobalValue>(b->getPointerOperand())) {
                            if (LoadInst *l = dyn_cast<LoadInst>(mdr.getInst())) {
                                assert(l->getPointerOperand() == b->getPointerOperand());
                            }
                        }
                        
                        stores.push(b);
                        //inv.visitStoreInst(*b);
                    }
                }
                
                StoreInst * cur;
                while (stores.size()) {
                    cur = stores.top();
                    stores.pop();
                    
                    inv.visitStoreInst(*cur);
                }
                
                for (BasicBlock::iterator j = fi->begin(), k = fi->end(); j != k; ++j) {
                    if (TerminatorInst *t = dyn_cast<TerminatorInst>(j)) {
                        inv.visitTerminatorInst(*t);
                    }
                }
                
                errs() << "BASIC BLOCK\n";
                errs() << *block << "\n";
            }
            
            /*
             for (inst_iterator I = inst_begin(rev), E = inst_end(rev); I != E; ++I) {
             if (StoreInst *b = dyn_cast<StoreInst>(&*I)) {
             negateStoreInstruction(b);
             }
             }
             */
            
            
            return true;
            
            target->viewCFG();
            
            // NEW STUFF HERE
            
#if 0
            for (Function::iterator i = target->begin(), e = target->end(),
                 I = rev->begin(), E = rev->end(); i != e; ++i) {
                // Print out the name of the basic block if it has one, and then the
                // number of instructions that it contains
                errs() << "Basic block (name=" << i->getName() << ") has "
                << i->size() << " instructions.\n";
                
                eraseBlockContents(i);
                
                instructionStack.clear();
                
                for (BasicBlock::iterator j = i->begin(), f = i->end(),
                     J = I->begin(), F = I->end(); j != f; ++j, ++J) {
                    // The next statement works since operator<<(ostream&,...)
                    // is overloaded for Instruction&
                    errs() << *J << "\n";
                    
                    if (isa<LoadInst>(J) || isa<StoreInst>(J)) {
                        errs() << "bypassing load / store\n";
                        errs() << *j << "\n\n";
                        // We need the mapping in place for proper lookups!
                        duals.insert(std::make_pair(J, j));
                    }
                    else if (isa<TerminatorInst>(J)) {
                        errs() << "bypassing terminator\n";
                    }
                    else {
                        errs() << "We need to reverse " << *J << "\n\n";
                        instructionStack.push_back(J);
                        outputList(instructionStack, "INSTRUCTION STACK");
                        
                        // We also need to map these two together - at this point
                        // they should have identical contents
                        duals.insert(std::make_pair(J, j));
                    }
                }
                
                reverseInstructionStack.clear();
                for (std::list<Instruction*>::iterator it = instructionStack.begin();
                     it != instructionStack.end(); ++it) {
                    reverseInstructionStack.push_back(*it);
                }
                
                // At this point, all of the Instructions we intend to reverse are
                // stored in instructionStack.  We do NOT store Loads/Stores/Terminators
                
                
                /*
                 Just brain storming here in comments...  So I think what's happening is that
                 we are swapping instructions, and some of those instructions were in the stack
                 and so now are deleted, which is a problem...  We may need to go from the front()
                 of the stack and the end() iterator to replace them...
                 OK so things are still getting squashed...  Create a map between every (read-only)
                 value from the original code and its (volatile) equivalent and look them up
                 */
                
                while (instructionStack.size()) {
                    transmute();
                }
                
                return true;
                
                BasicBlock::iterator j = i->begin();
                
                while (j != i->end()) {
                    Instruction *J = dyn_cast<Instruction>(j);
                    
                    errs() << "BasicBlock::iterator: " << *j << "\n";
                    
                    if (isa<LoadInst>(J) || isa<StoreInst>(J)) {
                        //errs() << "bypassing load / store\n";
                        //errs() << *j << "\n\n";
                        continue;
                    }
                    else if (isa<TerminatorInst>(J)) {
                        // Nothing!
                        continue;
                    }
                    else {
                        errs() << "instructionStack.size() is " << instructionStack.size() << "\n";
                        //Instruction *r = instructionStack.back();
                        //instructionStack.pop_back();
                        //errs() << "swapping " << *J << "with " << *r << "\n\n";
                        
                        // We have to do this here so we don't lose a valid iterator
                        ++j;
                        
                        transmute();//J);
                        outputList(instructionStack, "INSTRUCTION STACK");
                        
                        //errs() << "transmute returned " << *t << "\n";
                        
                        target->viewCFG();
                        
                        continue;
                    }
                    ++j;
                }
            }
#endif // 0
            
            target->viewCFG();
            
            return true;
            
            // NEW STUFF HERE
            
            
            // Find the exit block from OverFunc
            BasicBlock *oldBB = findExitBlock(*rev);
            //BasicBlock *newEntryBB = BasicBlock::Create(M.getContext(), "r_" + oldBB->getName(), target);
            ValueToValueMapTy VMap;
            BasicBlock *newEntryBB = CloneBasicBlock(oldBB, VMap, "", target);
            bbmNewToOld.insert(std::make_pair(newEntryBB, oldBB));
            
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
                bbmNewToOld.insert(std::make_pair(newBB, bb));
                bbmOldToNew.insert(std::make_pair(bb, newBB));
            }
            
            //target->viewCFG();
            
            for (std::map<BasicBlock *, BasicBlock *>::iterator it = bbmNewToOld.begin(), e = bbmNewToOld.end();
                 it != e; ++it) {
                errs() << *(*it).first << ": " << *(*it).second << "\n";
            }
            
            errs() << "**********************************\n\n";
            
            for (std::map<BasicBlock *, BasicBlock *>::iterator it = bbmNewToOld.begin(), e = bbmNewToOld.end(); 
                 it != e; ++it) {
                //errs() << *(*it).first << ": " << *(*it).second << "\n";
                reverseBlockInto((*it).second, (*it).first);
            }
            
            
        }
        return true;
    }
}

char Hello::ID = 0;
static RegisterPass<Hello> X("hello", "Hello World Pass");

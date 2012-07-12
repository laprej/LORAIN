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
    static cl::opt<std::string> FuncToInstrument("rev-func",
										 cl::desc("<func to reverse>"));//, llvm::cl::Required);	
	
	static cl::opt<std::string> FuncToGenerate("tgt-func",
										   cl::desc("<func to output>"));//, llvm::cl::Required);
	
	std::map<BasicBlock *, BasicBlock *> bbmOldToNew;
	
#pragma mark
#pragma mark Instrumenter

#undef DEBUG_TYPE
#define DEBUG_TYPE "Instrumenter"
    
	/// This class will apply the necessary instrumentation on the forward path
	class Instrumenter : public InstVisitor<Instrumenter>
	{
		Module &M;
        Hello *h;
        
        unsigned bitFieldCount;// = 0;
        
	public:
		Instrumenter(Module &mod, Hello *H): M(mod), h(H) {
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
            
            DEBUG(errs() << "Instrumenter: newBitFieldName is returning " << bff << "\n");
            
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
                    DEBUG(errs() << "Instrumenter: B->getSuccessor(0) is " << then->getName() << "\n");
					BasicBlock *el = B->getSuccessor(1);
                    DEBUG(errs() << "Instrumenter: B->getSuccessor(1) is " << el->getName() << "\n");
                    //B->getParent()->getParent()->viewCFG();
                    
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
        
        void throwOutPred(SmallVector<BasicBlock *, 4> &Preds)
        {
            assert(Preds.size() < 4 && "Don't yet support arb. size preds!");
            
            if (h->findDom(Preds[0]) == h->findDom(Preds[1])) {
                Preds.erase(&Preds[2]);
                return;
            }
            if (h->findDom(Preds[0]) == h->findDom(Preds[2])) {
                Preds.erase(&Preds[1]);
                return;
            }
            if (h->findDom(Preds[1]) == h->findDom(Preds[2])) {
                Preds.erase(&Preds[0]);
                return;
            }
        }
		
		void visitCmpInst(CmpInst &I) {
			/*
             * What we need to do here is emit a new global variable and assign
             * it the result of the cmp instruction in the function to be
             * reversed.
             */
            DEBUG(errs() << "Instrumenter: COMPARE INSTRUCTION\n");
			
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
			
			DEBUG(errs() << "Instrumenter: ll has type: " << *ll->getType() << "\n");
			DEBUG(errs() << "Instrumenter: I has type: " << *I.getType() << "\n");
			
			Value *lll = b.CreateCast(Instruction::Trunc, ll, I.getType());
			markJML(lll);
			
			DEBUG(errs() << "Instrumenter: lll has type: " << *lll->getType() << "\n");
			
			Value *v = b.CreateNUWAdd(lll, &I);
			markJML(v);
			
			DEBUG(errs() << "Instrumenter: got here...\n");
			
			Value *llll = b.CreateCast(Instruction::SExt, v, ll->getType());
			markJML(llll);
			
			DEBUG(errs() << "Instrumenter: llll has type: " << *llll->getType() << "\n");
			
			markJML(b.CreateStore(llll, l));
			
			
			DEBUG(errs() << "Instrumenter: Here, too!\n");
            
            
            
            //////////////// BIG CHANGE HERE //////////////////////
            /// Create a diamond!  Unfortunately llvm will not generate
            /// a merge point with just two preds (it just adds more on)
            /// so we'll need to add another BB after the merge
            BasicBlock *bb = h->postdomTreeLookup(I.getParent());
            
            DEBUG(errs() << "Instrumenter: PostDom of " << I.getParent()->getName()
                  << " is " << bb->getName() << "\n");
            
            int pcount = 0;
            
            SmallVector<BasicBlock *, 4> Preds;
            
            // Count predecessors
            for (pred_iterator PI = pred_begin(bb), E = pred_end(bb); PI != E; ++PI) {
                BasicBlock *Pred = *PI;
                DEBUG(errs() << "Instrumenter: pred[" << pcount << "] is " << Pred->getName() << "\n");
                Preds.push_back(Pred);
                pcount++;
                // ...
            }
            
            if (pcount < 3) {
                return;
            }
            
            // Throw out one of the preds
            if (Preds.size() > 2) {
                throwOutPred(Preds);
            }
            
            DEBUG(errs() << "Instrumenter: We have " << Preds.size() << " predecessors\n");
            
            llvm::SplitBlockPredecessors(bb, Preds.data(), Preds.size(), "_diamond");
		}
	};
    
    class Hello;
    
#pragma mark
#pragma mark Inverter

#undef DEBUG_TYPE
#define DEBUG_TYPE "Inverter"
    
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
			DEBUG(errs() << "\n\n\nInverter: LOOKUP\n");
			std::map<Value *, Value *>::iterator it;
			
			it = oldToNew.find(k);
			if (it == oldToNew.end()) {
				DEBUG(errs() << "Inverter: " << *k << " not found, outputting map\n");
				outputMap();
				return k;
			}
			
			return it->second;
		}
		
		void outputMap() {
			std::map<Value *, Value *>::iterator it, e;
			
			DEBUG(errs() << "Inverter: MAP BEGIN\n");
			for (it = oldToNew.begin(), e = oldToNew.end(); it != e; ++it) {
				DEBUG(errs() << "Inverter: key: " << *it->first << "\n\tdata: " << *it->second << "\n");
			}
			DEBUG(errs() << "Inverter: MAP END\n");
		}
        
        MDNode * findDiamondBf(Instruction &I)
        {
            DEBUG(errs() << "Inverter: findDiamondBf(" << I << "): ");
            MDNode * md = h->domTreeLookup(I);
            if (md) {
                DEBUG(errs() << *md << "\n\n");
            }
            else {
                DEBUG(errs() << "NULL\n\n");
            }
            
            return md;
        }
        
        /// Starting at start, can we arrive at target?
        /// We may need to revisit this when loop support is added
        bool bbDFS(BasicBlock *start, BasicBlock *target, int indent = 0)
        {
            if (indent == 0) {
                DEBUG(errs() << "\nInverter: bbDFS(" << start->getName() << ", " << target->getName() << ")\n");
            }
            else {
                DEBUG(errs() << 2 * ' ' << "\nbbDFS(" << start->getName() << ", " << target->getName() << ")\n");
            }
            
            if (start == target) {
                return true;
            }
            
            for (succ_iterator SI = succ_begin(start), E = succ_end(start); SI != E; ++SI) {
                BasicBlock *Succ = *SI;
                DEBUG(errs() << "Inverter: succ of " << start->getName() << ": " << Succ->getName() << "\n");
                if (target == Succ) {
                    return true;
                }
                if (true == bbDFS(Succ, target)) {
                    return true;
                }
            }
            
            return false;
        }
        
        /// bb1 is a basic block which terminates with a branch instruction.
        /// find the "true" branch and then check if bb2 is a descendant
        /// We may need to revisit this when loop support is added
        bool isTrueAncestor(BasicBlock * ancestor, BasicBlock * descendant)
        {
            BasicBlock *dom = h->findDom(ancestor);
            
            // Now we have to find the BB for "true" in our br instruction
            if (BranchInst *b = dyn_cast<BranchInst>(dom->getTerminator())) {
                BasicBlock *then = b->getSuccessor(0);
                if (bbDFS(then, descendant)) {
                    return true;
                }
                else {
                    return false;
                }
            }
            else {
                assert(0 && "Expecting a BranchInst from dominator!");
            }
        }
		
		void visitTerminatorInst(TerminatorInst &I) {
			DEBUG(errs() << "\n\n\nInverter: TERMINATOR INSTRUCTION\n");
            DEBUG(errs() << "Inverter: For " << I.getParent()->getName() << "\n");
			
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
			
            std::vector<BasicBlock *> bbv;
            
            // bb is the original basic block to be reversed
			BasicBlock *bb = I.getParent();
            
            for (pred_iterator PI = pred_begin(bb), E = pred_end(bb); PI != E; ++PI) {
                BasicBlock *Pred = *PI;
                DEBUG(errs() << "Inverter: pred of " << bb->getName() << ": " << Pred->getName() << "\n");
                // ...
                //pred_list.push_back(Pred);
                
                bbv.push_back(Pred);
                
                count++;
            }
			
            // foo is the generated "inverse" basic block
			BasicBlock *foo = builder.GetInsertBlock();
			
			DEBUG(errs() << "Inverter: count is " << count << "\n");
			
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
                
//				NamedMDNode *nmd = M.getNamedMetadata("jml.icmp");
//				assert(nmd);
//				errs() << "NMD: " << nmd->getName();// << "\n";
//				errs() << " has " << nmd->getNumOperands() << " operands\n";
				//MDNode *md = nmd->getOperand(0);
                
                MDNode *md = findDiamondBf(I);
				
                DEBUG(errs() << "Inverter: " << md->getName() << " has " << md->getNumOperands() << " operands\n");
				MDString *then = dyn_cast_or_null<MDString>(md->getOperand(0));
				assert(then);
				MDString *el   = dyn_cast_or_null<MDString>(md->getOperand(1));
				assert(el);
                ConstantInt *bfn = dyn_cast_or_null<ConstantInt>(md->getOperand(2));
                assert(bfn);
				/// We have two predecessors; we're going to need an "if"
				DEBUG(errs() << "Inverter: WE NEED A BRANCH\n");
				/// Emit a load of bf, compare, and jump to appropriate BBs
				
				/// CURRENTLY WORKING ON THIS
				/// Manually emit the load & cmp the bf
				/// Don't forget to tag all this JML!
                Type *newGlobal = IntegerType::get(I.getContext(), 32);
                Twine bfX("bf");
                bfX = bfX.concat(Twine(bfn->getZExtValue()));
                DEBUG(errs() << "Inverter: bfn->getZExtValue() returned " << bfn->getZExtValue() << "\n");
                DEBUG(errs() << "Inverter: and bfX is " << bfX << "\n");
				Value *l = M.getOrInsertGlobal(bfX.str(), newGlobal);
				//markJML(l);
				
				Value *ll = builder.CreateLoad(l);
				//markJML(ll);
				
				DEBUG(errs() << "Inverter: ll has type: " << *ll->getType() << "\n");
				DEBUG(errs() << "Inverter: I has type: " << *I.getType() << "\n");
				
				Value *lll = builder.CreateCast(Instruction::Trunc, ll, IntegerType::get(I.getContext(), 1));
				//markJML(lll);
				
				DEBUG(errs() << "Inverter: lll has type: " << *lll->getType() << "\n");
				
				Value *llll = builder.CreateICmpEQ(lll, builder.getInt1(true));
                
                /// Reset the bitfield
                Value *lllll = builder.CreateSub(ll, ll);
                /// Store back the bitfield
                Value *llllll = builder.CreateStore(lllll, l);
                
                if (isTrueAncestor(bb, bbv[0])) {
                    // Nothing is necessary, our bbv array is in correct order
                }
                else {
                    BasicBlock *temp = bbv[0];
                    bbv[0] = bbv[1];
                    bbv[1] = temp;
                }
				
				BasicBlock *thenBB = 0, *elBB = 0;
				Function *F = foo->getParent();
				/// Find the "then" block
				Function::iterator it, E;
				for (it = F->begin(), E = F->end(); it != E; ++it) {
					//std::string str = then->getString();
                    std::string str = bbv[0]->getName();
					str += "_rev";
					DEBUG(errs() << "Inverter: Comparing " << str << " and " << it->getName());
					if (str == it->getName()) {
						DEBUG(errs() << " Got it!\n");
						thenBB = it;
					}
					DEBUG(errs() << "\n");
					
					//std::string str2 = el->getString();
                    std::string str2 = bbv[1]->getName();
					str2 += "_rev";
					DEBUG(errs() << "Inverter: Comparing " << str2 << " and " << it->getName());
					if (str2 == it->getName()) {
						DEBUG(errs() << " Got it!\n");
						elBB = it;
					}
					DEBUG(errs() << "\n");
				}
//                for (it = F->begin(), E = F->end(); it != E; ++it) {
//                    if (bbv[0] == it) {
//                        thenBB = it;
//                        break;
//                    }
//                }
//                for (it = F->begin(), E = F->end(); it != E; ++it) {
//                    if (bbv[1] == it) {
//                        elBB = it;
//                        break;
//                    }
//                }
                
				
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
			
			DEBUG(errs() << "Inverter: I is a " << I << "\n");
			
			//assert(0 && "Unhandled terminator instruction!\n");
		}
		
		void visitAllocaInst(AllocaInst &I) {
			DEBUG(errs() << "\n\n\nInverter: ALLOCA INSTRUCTION\n");
			
			Value *alloc = builder.CreateAlloca(I.getType(), 0, I.getName());
			DEBUG(errs() << "Inverter: Allocating a " << *I.getType() << "\n");
			
			oldToNew[&I] = alloc;
		}
        
        void visitStoreInst(StoreInst &I) {
            DEBUG(errs() << "\n\n\nInverter: STORE INSTRUCTION\n");
			
			std::vector<Value *> bucket;
			oldToNew.clear();
            
            Value *storeVal = I.getPointerOperand();
			if (isa<GlobalValue>(storeVal)) {
                DEBUG(errs() << "Inverter: " << storeVal->getName() << " is a global value\n");
				currently_reversing = true;
            }
            else {
                DEBUG(errs() << "Inverter: " << storeVal->getName() << " is not a global value\n");
				currently_reversing = false;
            }
			
			getUseDef(&I, bucket);
			
			DEBUG(errs() << "Inverter: " << I << "\n");
			DEBUG(errs() << "Inverter: Bucket contains:\n");
			for (std::vector<Value *>::iterator it = bucket.begin(), e = bucket.end();
				 it != e; ++it) {
				DEBUG(errs() << "Inverter: " << **it << "\n");
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
			DEBUG(errs() << "\n\n\nInverter: LOAD INSTRUCTION\n");
			
			Value *valOfI = &I;
			
			Value *loadVal = I.getPointerOperand();
			if (isa<GlobalValue>(loadVal)) {
				DEBUG(errs() << "Inverter: " << loadVal->getName() << " is a global value\n");
			}
			else {
				DEBUG(errs() << "Inverter: " <<loadVal->getName() << " is not a global value\n");
			}
			
			/// Create a new load
			LoadInst *inst = builder.CreateLoad(loadVal);
			
			oldToNew[valOfI] = inst;
			outputMap();
			DEBUG(errs() << "\n\n\nInverter: LOAD INSTRUCTION END\n");
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
    
    BasicBlock * Hello::findDom(BasicBlock *BB)
    {
        Function *f = BB->getParent();
        DominatorTree &DT = getAnalysis<DominatorTree>(*f);
        DomTreeNode *Rung = DT.getNode(BB);
        Rung = Rung->getIDom();
        //errs() << "Rung is " << Rung << "\n";
        BasicBlock *bb = Rung->getBlock();
        //errs() << "BB is " << *bb << "\n";
        return bb;
    }
    
    
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
        errs() << "BB is " << bb->getName() << "\n";
        
        CmpInst *C = 0;
        
        // find the cmp
        for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
            if ((C = dyn_cast<CmpInst>(i))) {
                errs() << *C << "\n";
                break;
            }
        }
        
        assert(C && "CmpInst not found!");
        
        MDNode *md = C->getMetadata("jml.icmp");
        
        assert(md && "No metadata found on CmpInst!");
        
        return md;
    }
    
    BasicBlock * Hello::postdomTreeLookup(BasicBlock *BB)
    {
        Function *f = BB->getParent();
        PostDominatorTree &DT = getAnalysis<PostDominatorTree>(*f);
        DomTreeNode *Rung = DT.getNode(BB);
        Rung = Rung->getIDom();
        errs() << "PD Rung is " << Rung << "\n";
        BasicBlock *bb = Rung->getBlock();
        errs() << "PD BB is " << *bb << "\n";
        return bb;
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
                //IRBuilder<> build();
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
        Constant *temp = M.getOrInsertFunction(FuncToGenerate,
                                               Type::getVoidTy(M.getContext()),
                                               (Type *)0);
        
        if (!temp) {
            DEBUG(errs() << "We have a problem\n");
            exit(-1);
        }
        
        Function *reverse = cast<Function>(temp);
        if (reverse) {
            DEBUG(errs() << "OK supposedly we inserted " << FuncToGenerate << "\n");
        }
        else {
            DEBUG(errs() << "Problem inserting function " << FuncToGenerate << "\n");
            exit(-1);
        }
        
        ValueToValueMapTy vmap;
        SmallVectorImpl<ReturnInst*> Returns(1);
        CloneFunctionInto(reverse, M.getFunction(FuncToInstrument), vmap, true, Returns, "_reverse");
        
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
    
//    /// Negate a binary operator and all of its deps.
//    /// Use a builder to handle it.
//    void Hello::negateStoreInstruction(StoreInst *b) {
//        std::vector<Value *> bucket;
//        //getUseDef(b, bucket);
//        
//        errs() << "\n\n\nAfter negateStoreInstruction bucket holds:\n";
//        std::vector<Value *>::iterator it = bucket.end(), begin = bucket.begin();
//        --it;
//        
//        int num = 0;
//        
//        std::vector<Value *>::reverse_iterator rit(bucket.end()), re(bucket.begin());
//        for (; rit != re; ++rit) {
//            errs() << num++ << ": " << "(" << b->getParent()->getName() << ") " << **rit << "\n";
//        }
//        
//    }
    
    void Hello::getAnalysisUsage(AnalysisUsage &Info) const
    {
        //Info.addRequired<AliasAnalysis>();
        Info.addRequired<MemoryDependenceAnalysis>();
        Info.addRequired<PostDominatorTree>();
        Info.addRequiredTransitive<DominatorTree>();
        Info.addRequired<LoopInfo>();
        //Info.addRequiredTransitive<MemoryDependenceAnalysis>();
    }
    
    //virtual bool
    
    bool Hello::runOnModule(Module &M) {
        if (Function *ForwardFunc = M.getFunction(FuncToInstrument)) {
            ////////////////////////////////////////
            /// Instrument the forward event handler
            ////////////////////////////////////////
            Instrumenter instrumenter(M, this);
            for (inst_iterator I = inst_begin(ForwardFunc), E = inst_end(ForwardFunc); I != E; ++I)
                instrumenter.visit(&*I);
            
            DEBUG(errs() << "Found " << FuncToInstrument << "\n");
            
            Function *target = createReverseFunction(M);
            
            target->deleteBody();
            
            //std::map<BasicBlock*,BasicBlock*> oldToNew;
            //std::map<BasicBlock*,BasicBlock*> oldToNewRev;
            
            /// Put exit BB first in function
            BasicBlock *bb = findExitBlock(*ForwardFunc);
            BasicBlock *newBlock = BasicBlock::Create(getGlobalContext(),
                                                      "return_rev", target);
            bbmOldToNew[bb] = newBlock;
            bbmNewToOld[newBlock] = bb;
            
            /// Make analogs to all BBs in function
            Function::iterator fi, fe;
            for (fi = ForwardFunc->begin(), fe = ForwardFunc->end(); fi != fe; ++fi) {
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
            
            LoopInfo &LI = getAnalysis<LoopInfo>(*ForwardFunc);
            std::vector<BasicBlock*> loopBBs;
            
            for (fi = ForwardFunc->begin(), fe = ForwardFunc->end(); fi != fe; ++fi) {
                /// Create our BasicBlock
                //BasicBlock *block = BasicBlock::Create(getGlobalContext(),
                //									   "", target);
                
                // If our old block is in a loop, skip for now
                if (LI.getLoopDepth(fi)) {
                    errs() << fi->getName() << " is in a loop, bailing out\n";
                    loopBBs.push_back(fi);
                    //continue;
                }
                
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
                        
                        MemoryDependenceAnalysis& mda = getAnalysis<MemoryDependenceAnalysis>(*ForwardFunc);
                        
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
                
                inv.visitTerminatorInst(*fi->getTerminator());
                
                errs() << "BASIC BLOCK: ";
                errs() << block->getName() << "\n";
            }
            
            /*
             for (inst_iterator I = inst_begin(rev), E = inst_end(rev); I != E; ++I) {
             if (StoreInst *b = dyn_cast<StoreInst>(&*I)) {
             negateStoreInstruction(b);
             }
             }
             */
            
            //rev->viewCFG();
            
            return true;
            
            target->viewCFG();
            
            // NEW STUFF HERE
            
            target->viewCFG();
            
            return true;
            
            // NEW STUFF HERE
            
            
            // Find the exit block from OverFunc
            BasicBlock *oldBB = findExitBlock(*ForwardFunc);
            //BasicBlock *newEntryBB = BasicBlock::Create(M.getContext(), "r_" + oldBB->getName(), target);
            ValueToValueMapTy VMap;
            BasicBlock *newEntryBB = CloneBasicBlock(oldBB, VMap, "", target);
            bbmNewToOld.insert(std::make_pair(newEntryBB, oldBB));
            
            // Create all the new (empty) BBs
            for (Function::iterator i = ForwardFunc->begin(), e = ForwardFunc->end(); i != e; ++i) {
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

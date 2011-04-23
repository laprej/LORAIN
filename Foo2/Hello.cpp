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
//
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

/* // If a store Instruction is found
if (StoreInst* storeInst = dyn_cast<StoreInst>((*UI))) {
	cerr << " ### StoreFOUND " << *storeInst << "\n";
	Instruction &stInst = *Keep;
	
	// if the first operand is an instruction
	if (Instruction *Op = dyn_cast<Instruction>(stInst.getOperand(0)))	{
		
		IterateOverDef(Op);  // Iterate over the definitions of this Instruction
		
	}
}
 http://thread.gmane.org/gmane.comp.compilers.llvm.devel/11341/focus=11350
	 */

/* General outline of code:
 
 Create our ''foo'' module, which in the code is named ''Hello''
 
 ''foo'' uses our class ''Inverter'' which does the following:
 1. On the specified function, hunt for store instructions
 2. Upon finding a store instruction in 1), issue a load
 3. Recursively find its dependent instructions
 4. One by one invert each of the instructions and emit its inversion
 5. Emit a final store
 
 See the table in ``Efficient optimistic parallel simulations using reverse 
 computation'' by Carothers et al.
 */
	
namespace {
	static cl::opt<std::string> OverFunc("rev-func",
										 cl::desc("<func to reverse>"));//, llvm::cl::Required);	
	
	static cl::opt<std::string> TargetFunc("tgt-func",
										   cl::desc("<func to output>"));//, llvm::cl::Required);
	
	std::list<Instruction *> instructionStack;
    
    // Forward control flow through the CFG is determined by specific instructions.
    // Reverse control flow through the reverse CFG should be built up using the same
    // instruction type as the forward CF e.g. the dual to an ``if'' stmt will be
    // an ``if'' stmt
    struct dual_info {
        Value *forwardControl;
        Value *negatedControl;
        Value *testValue;
    };
	
	class Inverter : public InstVisitor<Inverter>
	{
		Module &M;
		
		IRBuilder<> &builder;
		
		Value *lastVal;
        
    public:
        Value *targetRestore;
		
	public:
		Inverter(Module &mod, IRBuilder<> &b): M(mod), builder(b) {}
		
		// Collect all use-defs into a container
		void getUseDef(User *I, int indent, std::vector<Value *> &bucket) {
			if (indent == 0) {
				DEBUG(errs() << "uses\n");
			}
			
            int uses = 0;
            
			for (User::op_iterator i = I->op_begin(), e = I->op_end();
				 i != e; ++i) {
				uses++;
                
				Value *v = *i;
				
				if (Instruction *w = dyn_cast<Instruction>(v)) {
					DEBUG(errs() << "outputing instruction dependent: " << *w << '\n');
					bucket.push_back(w);
					getUseDef(w, indent + 1, bucket);
                    DEBUG(errs() << "bucket now has " << bucket.size() << " elements\n");
				} 
                else if(BasicBlock *b = dyn_cast<BasicBlock>(v)) {
                    DEBUG(errs() << "BasicBlock\n\n");
                }
                else {
                    DEBUG(errs() << "This is a " << v->getType()->getDescription() << "\n");
                }


				/*
				else if (Operator *w = dyn_cast<Operator>(v)) {
					getUseDef(w, indent + 1);
				} else if (Constant *w = dyn_cast<Constant>(v)) {
					getUseDef(w, indent + 1);
				}
				 */
			}
            DEBUG(errs() << "This instruction has " << uses << " uses\n\n");
		}
        
        void visitICmpInst(ICmpInst &I)
        {
            DEBUG(errs() << "INTEGER COMPARE INSTRUCTION\n");
            
            BasicBlock *pb = I.getParent();
            Instruction *pi = &I;
            Value *L = &I;
            
            
            errs() << "Here\n";
            
            IRBuilder<> bar(pb);
            errs() << "Here\n";
            Instruction *term = pb->getTerminator();
            errs() << "Here\n";
            BasicBlock::iterator foo(term);
            errs() << "Here\n";
            bar.SetInsertPoint(pb, foo);
            errs() << "Here\n";
            
            
            const Type *newGlobal = IntegerType::get(I.getContext(), 32);
            Value *abc = M.getOrInsertGlobal("abc", newGlobal);
            DEBUG(errs() << "getOrInsertGlobal OK\n\n");
            /*
             GlobalVariable *foo = new GlobalVariable(newGlobal, false, 
             llvm::GlobalValue::InternalLinkage,
             0, "newVar");
             */
            GlobalVariable *GV = cast<GlobalVariable>(abc);
            
            GlobalVariable *GV1 = M.getNamedGlobal("abc");
            
            
            // Load
            Value *l1 = bar.CreateLoad(GV1, "abc");
            errs() << "Here\n";
            
            // CAST
            Value *L_cast = bar.CreateIntCast(L, IntegerType::get(I.getContext(), 32), true);
            // set
            Value *l2 = bar.CreateAdd(l1, L_cast);
            errs() << "Here\n";
            // store
            Value *l3 = bar.CreateStore(l2, GV1);
            errs() << "Here\n";
            
            //Instruction *newInst = new Instruction();
            
            //pb->getInstList().insert(pi, newInst); // Inserts newInst before pi in pb
        }
        
        void visitReturnInst(ReturnInst &I)
        {
        }
        
        void visitCmpInst(CmpInst &I)
        {
            /*
             * What we need to do here is emit a new global variable and assign
             * it the result of the cmp instruction in the function to be
             * reversed.
             */
            DEBUG(errs() << "COMPARE INSTRUCTION\n");
            /*
             GlobalVariable(const Type *Ty, bool isConstant, LinkageTypes Linkage,
                            Constant *Initializer = 0, const Twine &Name = "",
                            bool ThreadLocal = false, unsigned AddressSpace = 0);
             */
            
#if 0
            
            IRBuilder<> bar(I.getParent());
            
            bar.SetInsertPoint(I.getParent());
            
            // Create a new global
            const Type *newGlobal = IntegerType::get(I.getContext(), 32);
            Value *foo = M.getOrInsertGlobal("newVar", newGlobal);
            DEBUG(errs() << "getOrInsertGlobal OK\n\n");
            /*
            GlobalVariable *foo = new GlobalVariable(newGlobal, false, 
                                                     llvm::GlobalValue::InternalLinkage,
                                                     0, "newVar");
            */
            GlobalVariable *GV = cast<GlobalVariable>(foo);
            if (GV->hasInitializer()) {
                DEBUG(errs() << "Has init\n\n");
            }
            else {
                DEBUG(errs() << "Does NOT have init\n\n");
                GV->setLinkage(llvm::GlobalValue::InternalLinkage);
                GV->setConstant(false);
                GV->setInitializer(ConstantInt::get(bar.getInt32Ty(), 0, false));
            }

            DEBUG(errs() << "GlobalVariable created\n\n");
            Value *val = bar.CreateLoad(foo);
            DEBUG(errs() << "Load succeeded\n\n");
            /*
            Value *val = bar.CreateGEP(foo, 0);
            errs() << "GEP completed\n\n";
             */
            DEBUG(errs() << "type 1: " << val->getType()->getDescription());
            DEBUG(errs() << "type 2: " << I.getOperand(0)->getType()->getDescription());
            
            lastVal = bar.CreateAdd(val, ConstantInt::get(bar.getInt32Ty(), 1, false));
            lastVal = bar.CreateStore(lastVal, GV);
            //*/
            
#endif
            
        }
        
        void visitBranchInst(BranchInst &I)
        {
            DEBUG(errs() << "BRANCH INSTRUCTION\n");
        }
        
        void visitAllocaInst(AllocaInst &I)
        {
            DEBUG(errs() << "ALLOCA INSTRUCTION\n");
            DEBUG(errs() << "Name of alloca is " << I.getNameStr() << "\n");
            DEBUG(errs() << "It is of type " << I.getType()->getDescription() << "\n");
            //builder.CreateAlloca(I.getType(), 0, I.getNameStr());
            
            BasicBlock *pb = builder.GetInsertBlock();
            
            Instruction *pi = &pb->front();
            
            AllocaInst *alloc = new AllocaInst(I.getType(), I.getNameStr());
            
            pb->getInstList().insert(pi, alloc); // Inserts newInst before pi in pb
            
            //WriteBitcodeToFile(&M, outs());
            
        }
		
		void visitBinaryOperator(BinaryOperator &I) {
			DEBUG(errs() << "BINARY OPERATOR\n");
            
			if (I.getOpcode() == Instruction::Add) {
				DEBUG(errs() << "ADD INSTRUCTION\n");
                DEBUG(errs() << "lastVal: " << *lastVal << "\n");
                DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
				lastVal = builder.CreateSub(lastVal, I.getOperand(1));
			}
			
			if (I.getOpcode() == Instruction::Sub) {
				DEBUG(errs() << "SUB INSTRUCTION\n");
                DEBUG(errs() << "lastVal: " << *lastVal << "\n");
                DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
				lastVal = builder.CreateAdd(lastVal, I.getOperand(1));
			}
			
			if (I.getOpcode() == Instruction::Mul) {
				DEBUG(errs() << "MULT INSTRUCTION\n");
                DEBUG(errs() << "lastVal: " << *lastVal << "\n");
                DEBUG(errs() << "Operand 0: " << *I.getOperand(0) << "\n");
                DEBUG(errs() << "Operand 1: " << *I.getOperand(1) << "\n");
				lastVal = builder.CreateSDiv(lastVal, I.getOperand(1));
                //lastVal = builder.CreateSDiv(I.getOperand(0), I.getOperand(1));
			}
		}
		
		void visitStoreInst(StoreInst &I) {
			DEBUG(errs() << "STORE INSTRUCTION\n");
            
            Value *bar = I.getPointerOperand();
            errs() << "STORE FOR " << bar;
            // ASSUMPTION: Anything that is not a global ought to be a local
            if (GlobalValue *gv = dyn_cast<GlobalValue>(bar) ) {
                DEBUG(errs() << bar->getNameStr() << " is a global value\n");
            }
            else {
                // OK - we're working with a local variable, if this is where
                // the value is defined then great - otherwise we need to do
                // the use-def chain thing
                DEBUG(errs() << bar->getNameStr() << " is not a global - bypassing visitor\n");
                std::vector<Value *> UDC;
                getUseDef(&I, 0, UDC);
                //return;
            }

			//getUseDef(&I, 0);
			
			// OK.  We have a Store.  We need to do the following:
			//
			// 1. Create a Load
			// 2. Using use-def chains, find all of the instructions on which
			//    this value depends (and possibly store in a vector)
			// 3. For each instruction from (2), find all of their respective
			//    dependencies (and possibly store in a vector)
			// 4. It's time to ``unique'' all entries in said vector
			// 5. Pass all of the instructions in the vector to the inverter
			
			// We could try and wrap our visitor pattern in a function that
			// returns a Value * ala chapter 3 of the tutorials
			
			if (TargetFunc.length() < 1) {
				DEBUG(errs() << "Bad Target Function!\n");
				exit(-1);
			}
			
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
			
			/*
			reverse->setCallingConv(CallingConv::C);
			
			BasicBlock *block = BasicBlock::Create(getGlobalContext(),
												   "entry", reverse);
			IRBuilder<> builder(block);
			*/
			// 1. Create a Load
			Value *load = builder.CreateLoad(I.getPointerOperand());
			
			// 2. Using use-def chains, find all of the instructions on which
			//    this value depends (and possibly store in a vector)
			// use-def chain.  Used for tracking instruction deps
			std::vector<Value *> UDC;
			getUseDef(&I, 0, UDC);
			
			lastVal = load;
			
			// WE NEED TO WORK BACKWARDS i.e. CONSTRUCT OUR EARLIEST DEP FIRST
			/* std::vector<Value *>::iterator it, end;

			for (it = UDC.begin(), end = UDC.end(); it != end; ++it) {
				if (Instruction *inst = dyn_cast<Instruction>(*it)) {
					visit(inst);
				}
			} */
			
			DEBUG(errs() << "HERE\n");

			while (!UDC.empty()) {
				Value *cur = UDC.back();
                errs() << *cur << " ON USE-DEF STACK\n";
				UDC.pop_back();
				if (Instruction *inst = dyn_cast<Instruction>(cur)) {
                    errs() << "PROCESSING " << *inst << " FROM STACK\n";
					visit(inst);
				}
			}
            
            DEBUG(errs() << "HERE AS WELL\n");
			
			/* User::op_iterator i, j;
						
						for (i = I.op_begin(), j = I.op_end(); i != j; ++i) {
							UDC.insert(*i);
							if (Instruction *inst = dyn_cast<Instruction>(i)) {
								errs() << "The store depends on the instruction: " << *inst << '\n';
							}
							Value *x = *i;
							errs() << "The store depends on " << x->getType()->getDescription() << '\n';
						} */
			
			builder.CreateStore(lastVal, I.getPointerOperand());
			
			//builder.CreateRetVoid();
			
		}
		
		// This function MUST be overridden if the return type is changed
		void visitInstruction(Instruction &I) {}
		
	};
	
	// Hello - The first implementation, without getAnalysisUsage.
	struct Hello : public ModulePass {
		static char ID; // Pass identification, replacement for typeid
		Hello() : ModulePass(ID) {}
        /// Our Inverter class
        Inverter *inv;
        /// Our new reverse function
        Function *reverse;
        
        /**
         Add a flag to keep track of when we are pulling in dependencies.
         Make sure we don't pull in ourselves e.g. if we're loading x, don't
         pull another x.
         */
        
        ~Hello()
        {
            delete inv;
        }
        /**
         This method has to be overridden in order for everything to work
         */
        virtual void getAnalysisUsage(AnalysisUsage &Info) const
        {
            Info.addRequired<LoopInfo>();
            Info.addRequired<PostDominatorTree>();
        }
        
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
		
        void reverseAndEmit(Function &F, IRBuilder<> &builder)
        {
            //Inverter inv(*F.getParent(), builder);
            inv = new Inverter(*F.getParent(), builder);
            
            // idf = Inverse Depth First Search on basic blocks which is how
            // we're going to do this
            idf_iterator<BasicBlock *> I = idf_begin(findExitBlock(F));
            idf_iterator<BasicBlock *> E = idf_end(findExitBlock(F));

            for (; I != E; ++I) {
                Value *v = *I;
                static int count = 0;
                DEBUG(errs() << "Value " << count++ << ":\n");
                if (BasicBlock *bb = dyn_cast<BasicBlock>(v)) {
                    reverseBlock(bb);
                }
            }
            
            pred_iterator foo(findExitBlock(F));
            //F.viewCFG();
            //errs() << "pred_iterator: " << **foo << "\n";
            
            po_iterator<Function*> bar = po_iterator<Function*>::begin(&F);
            po_iterator<Function*> ebar = po_iterator<Function*>::end(&F);
            
            errs() << "\n";
            for (; bar != ebar; ++bar) {
                errs() << "err? " << **bar << "\n";
            }

            //F.viewCFG();
            
            
            GraphTraits<const BasicBlock*> bleh;
            GraphTraits<const BasicBlock*>::getEntryNode(&(F.getEntryBlock()));
            
            Inverse<Function*> whoa(&F);
            
            GraphTraits<Inverse<Function*> > bleah;// = GraphTraits<Inverse<const Function*> >::getEntryNode(Inverse<Function*> &F);
            GraphTraits<Inverse<Function*> >::getEntryNode(whoa);
            BasicBlock *f = GraphTraits<Inverse<Function*> >::getEntryNode(whoa);
            
            errs() << "f is " << *f << "\n";
            
            PostDominatorTree *PDT = &getAnalysis<PostDominatorTree>(F); // new PostDominatorTree;
            //PrintDom
            //PrintDomTree(const DomTreeNodeBase<NodeT> *N, raw_ostream &o,
            //             unsigned Lev)
            //PrintDomTree<BasicBlock>(PDT->DT, errs(), 0);
            //PDT->print(errs(), F.getParent());
            //DomTreeNodeBase<BasicBlock>::PrintDomTree(PDT, errs(), 0);
            //PDT->runOnFunction(F);
            errs() << "PostDominatorTree!!!\n";
            //PDT->print(errs(), F.getParent());
            df_iterator<PostDominatorTree*> pdt_it = df_begin(PDT);
            df_iterator<PostDominatorTree*> pdt_end = df_end(PDT);
            
            for (++pdt_it; pdt_it != pdt_end; ++pdt_it) {
                errs() << "Woo: " << *pdt_it << "\n";
                errs() << "In: " << pdt_it->getDFSNumIn();
                errs() << "   Out: " << pdt_it->getDFSNumOut() << "\n";
                errs() << "IDOM: " << pdt_it->getIDom() << "\n\n";
            }
            errs() << "End PostDominatorTree\n";
            
            F.viewCFG();
            
            /* LoopInfo * LI = &getAnalysis<LoopInfo>(F);
            DEBUG(errs() << "Ping\n");
            DEBUG(LI->print(errs()));
            DEBUG(errs() << "Pong\n");
            
            BasicBlock *exitBlock = findExitBlock(F);
            reverseBlock(exitBlock);
            
            for (pred_iterator PI = pred_begin(exitBlock),
                 E = pred_end(exitBlock); PI != E; ++PI) {
                BasicBlock *Succ = *PI;
                errs() << "Basic block (name=" << Succ->getName() << ") has "
                << Succ->size() << " instructions.\n";
            } */
            
            builder.CreateRetVoid();
        }
        
        /// runOnFunction - Given a function (and a module), we need to do
        /// the following: 1. Instrument the forward path through the given
        /// function (e.g. record which path was taken through an ``if''),
        /// and 2. Emit the proper reverse function that moves through the
        /// reverse-CFG following the hints we left behind
		bool runOnFunction(Function &F, Module &M) {
            // TODO: Instrument the forward code
            errs() << "This function's symbol table is:\n";
            ValueSymbolTable::iterator it = F.getValueSymbolTable().begin();
            ValueSymbolTable::iterator E  = F.getValueSymbolTable().end();
            for (; it != E; ++it) {
                errs() << (*it->second).getType()->getDescription() << ": ";
                errs() << it->first() << ": ";
                errs() << *it->second << "\n";
            }
            
			// We should probably create the reverse function here...  We
			// know we already have a valid forward function so even if we don't
			// add any instructions, it's still a valid reversal.
			
			Constant *temp = M.getOrInsertFunction(TargetFunc, 
												   Type::getVoidTy(M.getContext()),
												   (Type *)0);
			
			if (!temp) {
				DEBUG(errs() << "We have a problem\n");
				exit(-1);
			}
			
			//Function *reverse = cast<Function>(temp);
            reverse = cast<Function>(temp);
			if (reverse) {
				DEBUG(errs() << "OK supposedly we inserted " << TargetFunc << "\n");
			}
			else {
				return false;
			}
			
			reverse->setCallingConv(CallingConv::C);

            /// Create our BasicBlock
			BasicBlock *block = BasicBlock::Create(getGlobalContext(),
												   "rev_entry", reverse);
            
            /// And our IRBuilder to generate the body of the BB for us
			IRBuilder<> builder(block);
            
            // emit new, reversed basic blocks assuming a single exit node
            // which we will use as an entry block
            reverseAndEmit(F, builder);
            
            verifyFunction(*reverse);

			return true;
		}
		
		virtual bool runOnModule(Module &M) {
			DEBUG(errs() << "runOnModule: ");
			DEBUG(errs().write_escaped(M.getModuleIdentifier()) << '\n');
			
			DEBUG(errs() << "func_to_reverse is " << OverFunc << '\n');
			
			if (Function *rev = M.getFunction(OverFunc)) {
				DEBUG(errs() << "We found " << OverFunc << "!\n");
				
				runOnFunction(*rev, M);
                
                                verifyModule(M);
				
				// Output the bitcode file to stdout
				WriteBitcodeToFile(&M, outs());
				
				return true;
			}
			
			return false;
		}
	};
}

char Hello::ID = 0;
static RegisterPass<Hello> X("foo", "Foo Hello World Pass");

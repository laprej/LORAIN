add_subdirectory(BCGEPs)
add_subdirectory(AugmentStruct)

add_llvm_loadable_module( LLVMFoo4
  Foo4.h
  Hello.cpp
  )

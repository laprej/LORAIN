Pre-Generated BC files
======================

phold.bc
--------
mpicc  -DROSS_VERSION=680 -DUNITY_INCLUDE_64 -D_GNU_SOURCE -DROSS_OPTION_LIST='" -DUSE_AVL_TREE -DAVL_NODE_COUNT=262144 -DRAND_NORMAL -DROSS_timing -DROSS_QUEUE_splay -DROSS_RAND_clcg4 -DROSS_NETWORK_mpi -DROSS_CLOCK_amd64 -DROSS_GVT_mpi_allreduce -DARCH_x86_64"' -O0 -DNDEBUG -I/Users/laprej/TDD/rossnet/trunk/ross     -DUSE_AVL_TREE -DAVL_NODE_COUNT=262144 -DRAND_NORMAL -DROSS_timing -DROSS_QUEUE_splay -DROSS_RAND_clcg4 -DROSS_NETWORK_mpi -DROSS_CLOCK_amd64 -DROSS_GVT_mpi_allreduce -DARCH_x86_64 -o CMakeFiles/phold.dir/phold.c.o   -c /Users/laprej/TDD/rossnet/trunk/ross/models/phold/phold.c -emit-llvm -c -o phold.bc

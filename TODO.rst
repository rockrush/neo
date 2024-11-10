1. Supports linking into network
2. Show the current hierarchy of neural networks
3. Lifetime of neuron support
4. Automatically adds neuron, and fade out useless ones
5. Manual tuning of network
6. Network visualization
   QuickQanava seems to be a starting point
7. Supports CUDA/OpenCL/FPGA, which work as computing node, just like CPU
   different kind of nodes can co-exist, is data that flows
8. source level plugin also flows among nodes, if tested enough
   these plugins may offer new neuron type, new connection type, etc.
9. Port array per node:
   a. One router on each node, that supports different type of connections,
      for multi-node cluster, on-board accelerators, etc.
   b. Data and command to be formated into specific structure.
   c. Neuron can be simply addressed after IPv6, benifits existing network
      from computer system.
   d. Node failure causes partial death, not total failure. With kind of
      replication scheme, data won't lose, or rebuild the dead fraction without
      replication, to emulate brain growth.


https://github.com/Cyan4973/xxHash
inotify
epoll

+----------------------------------------------------------------------------------+
|  In[] --> -- Neuron --> Out[]
|         ^
|  In[] --|
|
|  data array: item_size, len, data[];
+----------------------------------------------------------------------------------+

1. management of pool
   create
   init/reinit
   destroy
   pause
   resume
2. management of thread
   create
   destroy
   pause
   resume
3. management of work
   create
   destroy
   pause
   resume
   attach
   detach
4. memory management
   alloc
   free
   init
   resize
   destroy

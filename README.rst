==========================================
Neo, named after *The Secret of Bluewater*
==========================================

Self-growing neural network, for possibly real AI applications

Connects any sensors and motors in a cluster, and generates a neuron network, to convert sensor inputs into motor actions.

Balance computing pressure among cluster, and deal with node drop & join.

-------
Packet:
-------
The API of communicate with an existing neuron network, not necessarily be API between neurons.

+-----+------+------+------+----------+
| dst | len  | type | flag | payload  |
+=====+======+======+======+==========+
| 8B  |  4B  |  1B  |  1B  |          |
+-----+------+------+------+----------+


------
flag
------
#. (1b)flow: packet to be passed to any neurons connected to the receiving one
#. (1b)propagate：activate data storage, and effects connected neurons
#. (1b)ensure: whether to retry if failed
#. (5b)reserved


----
type
----
Packet type, including neuron management, data flow, etc.
Neuron management includes query for neuron status, stop neuron, start neuron, delete neuron, create neuron, snapshot, etc.


---
len
---
Length of payload.


---
dst
---
Address of target neuron, uuid seems to be overlong, we need an effective addressing scheme, that supports neuron drifting, and various underlying connection type.


-------
payload
-------
Custom format according to `type`.


-----
TODO
-----
#. Unary operator: process data from one neuron, and pass on
#. Binary operator: add, subtract, mutiply, divide, ...
#. GUI app: client app to control neuron network, and for network monitoring
#. verification & encryption: stop hijacking in an open connecting environment
#. Matrix operator: even with deep learning nowadays
#. Matroid operator: final state, process on multi-modal input/output

# ReplicaWBCache
## LICENSE Issue
This project hasn't sovled LICENSE problem. **Evaluate the LICENSE risk before using this project in product**.  

## Introduction
In distributed storage system, both bandwidth and latency are limited by network performance in normal case. There's one idea to use host local fast storage device as write back cache to mitigate the network limitation to improve the bandwidth and latency performance.  
  
Without cache replication function, it makes none sense for this kind of design in distributed storage system. The target is to use RDMA semantics to implement messenger transaction among hosts/nodes to implement replication function.  
RDMA use scatter/gather element to send/receive message, so it's normal to use append log structure(redo log) to place the data in the cache device.  
Since replication is needed, we need use Paxos protocol for consensus. etcd is going to be used here.
  
ReplicWBCache is going to be implemented on local storage media AEP/DCPMM  

## Debug
For continuous optimization of this project, [lttng](https://lttng.org/docs/v2.11/) & [OpenTracing](https://opentracing.io/docs/overview/) is used in this project for trace & debug & optimization.  

## Code Style
To contribute to this project, please follow [Google C++ style](https://google.github.io/styleguide/cppguide.html).  
Note: This project use **__cplusplus 201703L**.  

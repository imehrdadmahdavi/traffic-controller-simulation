# Traffic Controller Simulation

## Summary

This program emulate/simulate a traffic shaper who transmits packets controlled by a token bucket filter depicted
below using multi-threading within a single process.

## System Architecture

![Token Bucket](figure1.png)

**Figure 1:** A system with a token bucket filter.

Figure 1. above depicts the system. The token bucket has a capacity (bucket depth) of B tokens. Tokens arrive into the token bucket according to an unusual arrival process where the inter-arrival time between two consecutive tokens is 1/r. r is called the token arrival rate. Extra tokens (overflow) would simply disappear if the token bucket is full. A token bucket, together with its control mechanism, is referred to as a token bucket filter.

Packets arrive at the token bucket filter according to an unusual arrival process where the inter-arrival time between two consecutive packets is 1/lambda. We will call lambda the packet arrival rate. Each packet requires P tokens in order for it to be eligible for transmission. (Packets that are eligible for transmission are queued at the Q2 facility.) When a packet arrives, if Q1 is not empty, it will just get queued onto the Q1 facility. Otherwise, it will check if the token bucket has P or more tokens in it. If the token bucket has P or more tokens in it, P tokens will be removed from the token bucket and the packet will join the Q2 facility, and wake up the servers in case they are sleeping. If the token bucket does not have enough tokens, the packet gets queued into the Q1 facility. Finally, if the number of tokens required by a packet is larger than the bucket depth, the packet must be dropped (otherwise, it will block all other packets that follow it).

The transmission facility (denoted as S1 and S2 in the above figure and they are referred to as the "servers") serves packets in Q2 in the first-come-first-served order and at a service rate of mu per second. When a server becomes available, it will dequeue the first packet from Q2 and start transmitting the packet. When a packet has received 1/mu seconds of service, it leaves the system. Also, the servers are kept as busy as possible.

The system can run in only one of two modes:

-**Deterministic:** In this mode, all inter-arrival times are equal to 1/lambda seconds, all packets require exactly P tokens, and all service times are equal to 1/mu seconds (all rounded to the nearest millisecond).

-**Trace-driven:** In this mode, we will drive the simulation using a trace specification file. Each line in the trace file specifies the inter-arrival time of a packet, the number of tokens it needs in order for it to be eligible
for transmission, and its service time.

The system simulates the packet and token arrivals, the operation of the token bucket filter, the first-come-first-served queues Q1 and Q2, and servers S1 and S2. It also produces and sent to the output a trace of the simulation for every important event occurred in the simulation. 



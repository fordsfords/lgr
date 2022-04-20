# lgr - Fast logger

This is an example of a reasonably low-impact logging
infrastructure.

## TABLE OF CONTENTS

<sup>(table of contents from https://luciopaiva.com/markdown-toc/)</sup>

## COPYRIGHT AND LICENSE

I want there to be NO barriers to using this code, so I am releasing it to the public domain.
But "public domain" does not have an internationally agreed-upon definition, so I use CC0:

Copyright 2022 Steven Ford http://geeky-boy.com and licensed
"public domain" style under
[CC0](http://creativecommons.org/publicdomain/zero/1.0/):
![CC0](https://licensebuttons.net/p/zero/1.0/88x31.png "CC0")

To the extent possible under law, the contributors to this project have
waived all copyright and related or neighboring rights to this work.
In other words, you can use this code for any purpose without any
restrictions.  This work is published from: United States.  The project home
is https://github.com/fordsfords/lgr

To contact me, Steve Ford, project owner, you can find my email address
at http://geeky-boy.com.  Can't see it?  Keep looking.

## REPOSITORY

See https://github.com/fordsfords/lgr for code and documentation.

## DEPENDENCIES

* [cprt](https://github.com/fordsfords/cprt) - C portability helper.
* [q](https://github.com/fordsfords/q) - Fast queue.

## INTRODUCTION

This logger is designed for high-performance,
low-latency applications that require low execution overhead.

### Features

Features important to high-performance:
* Zero malloc/free operation on application threads.
* Zero kernel calls on application threads.
* Zero thread contention between an application thread and the logger thread.
(Note however possible thread contention between multiple application threads
logging at the same time; see [Spinlocks](spinlocks).)
* High-precision microsecond timestamps using gettimeofday().

Other features:

### Not For Everybody

This logger package should work for pretty much any kind of application,
but may not always be the right choice.
Some of its design choices that make it good for high-performance,
low-latency applications could be objectionable for other kinds
of applications.

Specifically:
1. The design assumes an abundance of CPU resources (i.e. a many-core host).
It makes use of spinlocks and short-sleep polling,
both of which are objectionable in a CPU-constrained environment.
2. The design assumes an abundance of memory.
It pre-allocaates maximum-sized buffers,
which allows a reduction in thread contention.
But this design is wasteful of memory if the average log message size is
significantly less than maximum message size.

## DESIGN NOTES

### Spinlocks

Bad for cpu-constrained systems.
Could eliminate with [Per-Thread Queues](#per-thread-queues).

### Memory Waste

Memory footprint = (max msg size * q size)

non-contention with logger thread.

Could it use SMX design?

### msg overflow detection.

### q overflow. Counter vs flow control (blocking).

### autoflush

### Log rolling for 24x7 operation (Mon - Fri).
  * Max size per day,
    after which generates alert and stops logging till next day.

### Per-Thread Queues

More perf, but complexity in ordering.


# lgr - Fast logger

This is an example of a reasonably low-impact logging
infrastructure.

## INTRODUCTION

This logger is designed for high-performance,
low-latency applications that require low execution overhead.

## DEPENDENCIES

* [cprt](https://github.com/fordsfords/cprt) - C portability helper.
* [q](https://github.com/fordsfords/q) - Fast queue.

### Features

Features important to high-performance:
* 190 ns application thread execution time on 3.8 GHz Intel(R) i7-9800X,
logging simple string. See "lgr_perf.c".

* Zero malloc/free operation on application threads.

* Zero kernel calls on application threads.

* Zero thread contention between an application thread and the logger thread
during normal operation.
(Some possible contention if the application overloads and overflows the
logger queue.
Also note possible thread contention between multiple application threads
logging at the same time; see [Spinlocks](spinlocks).)

* High-precision microsecond timestamps using gettimeofday() (Unix) or
GetSystemTimeAsFileTime() (Windows).

* Weekday-based log file "rolling".
To prevent log files do not grow to infinity,
lgr keeps 7 log files, one for each day of the week.

* Size-based log file limit.
To prevent log files do not grow to infinity,
lgr stops writing if the file exceeds a specified size limit.
Note that lgr will write a warning when this happens,
including the number of log messages dropped due to file size limit.


Other Features:

* Tracks "overflows"
(when application is logging faster than the logging thread can write the logs).
Overflow is considered to be a problem in the application,
and the lgr package is designed to handle it gracefully.

* Intelligent flushing.
When the logging thread writes a message to the log file,
it checks to see if the queue has another message waiting.
If so, it does not flush the I/O stream.
But if the log queue is empty, it flushes.

* Flush on exit.
When the "lgr" object is deleted,
it empties the log queue before closing the file and returning.

* Optional no-lock flag for applications that can guarantee single-threaded
API usage. Slight performance improvement, but no longer thread-safe.

* Optional timestamp deferment to logger thread.
Slight performance improvement, but timestamp can be inaccurate in proportion
to queue length.
In some cases, the inaccuracy can be tens of milliseconds.

### Possible Unexpected Behaviours:

#### Message Ordering

Messages in the log file will sometimes appear to be out of order,
based on the timestamps.
This is not a bug.

It is due to the fact that lgr sometimes internally generates a message,
independently of the application.
For example, if a midnight crossing happens since the last logged message,
and the application logs something new,
lgr will write a "Closing" message to and close the previous day's log file,
and open a new one and write an "Opening" message to it.
Then it writes the application's message to the new file.

But note that the time stamp of the "Closing" and "Opening" messages will be
*after* the application's log message, since they were generated afterwards.

#### Messages In Wrong Day File

It is not possible for lgr to close the previous day's
file infinitesimally before  midnight and open the new one at exactly midnight.
Instead, lgr uses an application log to trigger checking for the midnight
crossing.
For example, if the first message of a new day is written at 1 am,
the "Closing" message written to the previous day's log file will have
the new day's 1 am timestamp.

#### Overwriting Existing Files

If an application is running and logging information,
and is then exited and restarted,
the current day's log file will be overwritten.

Some might prefer that restarting mid-day should simply start appending
to the existing log file, and only overwrite on a midnight crossing.
I can see how this would be a useful feature, but it is not implemented.

#### Overflow vs. File Size Drops

Overflows (writing logs too fast) and file size drops are similar in that
messages are lost and counts are maintained of the number of lost messages.
However, a log that overflows returns an error,
whereas a log that is dropped due to file size returns success.
Ideally file size drops would also return an error.

However, the log API does not have reliable visibility of file size drops.
For example, suppose that the file size is close to the limit,
and there are several messages in the log queue.
When a new log message is enqueued, the file size is not yet exceeded,
so it returns success.
But as log messages are dequeued, the file size can be exceeded,
such that the successful message is dropped.

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

### Overflows

Applications, especially high-performance applications,
should control the amount of logging they do.
Logs typically go to a file,
and writing to a file is typically slow compared to the maximum
throughput of high-performance applications.
If an application attempts to write log messages faster than the
disk can accept them, there is something wrong with the application.

However, it is not unusual for short bursts of log messages to
exceed the disk write speed.
This is not considered pathological,
as long as the average logging rate is within the disk speed.

One approach is to have the logger block the application when it is
logging too fast.
This is what happens if the application simply performs the
writes itself, perhaps by calling "fprintf()".
The application is slowed down by the act of logging.
But this is generally not acceptable for high-performance applications.

The lgr package uses an asynchronous thread to write to disk.
The application enqueues log messages to be written.
The size of the queue is specified at lgr creation time,
and should be large enough to handle worst-case bursts of logs.

In normal operation, the enqueue operation is very efficient and
does not block the application if the logging thread falls behind.
But if the application logs too much too quickly,
the log queue will fill up, or more accurately,
the pool of available log objects runs out.

If that happens, rather than blocking the application,
the lgr package enqueues a special "overflow" log,
and returns failure status to the application,
indicating that the message was *not* logged.
It also maintains a set of per-severity overflow counts.

When the special overflow log is dequeued by the logger thread,
it captures the per-severity overflow counts, takes a "current"
timestamp, and reports the counts and the time period.

#### Overflow Implementation Details

The log queue is made one larger than the number of free log structures.
This is to leave room for an "overflow" log structure,
which is allocated in the main logger object at lgr_t.overflow_log.

When an overflow happens, the call to "lgr_log()" is unable to get a lgr_log_t
from the pool. The lgr_t.overflow_lock is taken and the proper element in
the lgr_t.overflows[] array is incremented.
Then the lgr_t.overflow_log is enqueued and
lgr_t.overflow_log_available is set to false.

During the time that the overflow lgr_log_t is making its way to the head
of the queue, more overflows may happen.
But lgr_t.overflow_log_available set to false simply increments the
appropriate lgr_t.overflows[] array element.

When the overflow log makes it to the head of the queue,
the logger thread recognizes it as an overflow log
(log_t.type == LGR_LOG_TYPE_OVERFLOW) and takes the
lgr->overflow_lock for long enough to copy and zero out the
lgr_t.overflows[] array and set lgr_t.overflow_log_available to true.
Then the overflow message is logged, which indicates the timestamps
of the first overflow and the current time.

Note that the lgr_t.overflow_log contents is only used for its timestamp.

### Asserts

There are sanity checks for conditions that "should never happen" using
CPRT_ASSERT() (see "cprt.h").
The specific implementation of CPRT_ASSERT() may not be appropriate for
all applications; it writes to standard error and then calls "exit(1)".
You might prefer for it to generate a core dump.

### Locking

The lgr package needs to protect internal structures from multi-threaded access.

There are two locks used: "log_lock" and "overflow_lock".
The log_lock is used every time that a log is being enqueued to
protect the non-thread-safe queue.
The overflow_lock is only used when the caller is enqueuing logs faster than
the logging thread can dequeue, and the queue fills up (or rather the
pool empties).
Thus, under non-pathological operation, the overflow_lock is not used.

If the user can guarantee that lgr_log() will only ever be executed
by one thread at a time, the lgr object can be created with the
LGR_FLAGS_NOLOCK flag to suppress the creation and use of log_lock.
Note that the "overflow_lock" is always created and used,
even if LGR_FLAGS_NOLOCK is supplied.

The lgr currently uses spinlocks that resolve contention by busylooping instead
of traditional mutex locks that resolve contention by sleeping.

Spinlocks are good for low-latency use cases where threads don't compete for
CPU availability (e.g. allocating a core per thread).
But on a cpu-constrained host, spinlocks can lead to VERY long latencies
up to the scheduler time quantum (often 10 ms).

The locks can be eliminated if the design were modified to include
per-thread queues.

### Memory Waste

Memory footprint = (max msg size * q size)

non-contention with logger thread.

Could it use SMX design?

### Open output file

Log rolling for 24x7 operation (Mon - Fri).
* Max size per day,
    after which generates alert and stops logging till next day.

## Possible Changes

### Per-Thread Queues

Eliminates locks for message enqueuing, but increases complexity.

## COPYRIGHT AND LICENSE

I want there to be NO barriers to using this code, so I am releasing it to the public domain.  But "public domain" does not have an internationally agreed upon definition, so I use CC0:

This work is dedicated to the public domain under CC0 1.0 Universal:
http://creativecommons.org/publicdomain/zero/1.0/

To the extent possible under law, Steven Ford has waived all copyright
and related or neighboring rights to this work. In other words, you can 
use this code for any purpose without any restrictions.
This work is published from: United States.
Project home: https://github.com/fordsfords/lgr

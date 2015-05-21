# Introduction #

The pipeline execution framework
makes C++ pipeline computationes (nearly) as easy to write as in shell programs.
Pipelines manage functions executing in threads and communicating via queues.

The approach is to build the framework in layers,
from the basic facilities on up to is full power.
We describe the layers in outline below, but defer details to separate pages.

After describing the layers, we outline some example applications.

# Basic Layer #

The basic layer handles threads implicitly,
but requires explicit management of the queues.

Each function accepts the fronts and/or rears of queues,
writing to or reading from them, respectively.
(This approach to fronts and rears rather than full queues
enables late binding of queue types.)
The function contains a loop
reading from its upstream queues and writing to its downstream queues.

```
extern void f1(Queue::Front&); // i.e. producer
extern void f2(Queue::Rear&, Queue::Front&); // i.e. filter
extern void f3(Queue::Rear&); // i.e. consumer
```

A pipeline object controls execution,
creating threads as needed and waiting for all pipeline stages to exit.

```
Blocking_Queue q1, q2;
Pipeline p1;
p1.exec(f1, q1.front());
p1.exec(f2, q1.rear(), q2.front());
p1.exec(f3, q2.rear());
// implicit p1.wait() in destructor of Pipeline
```

When a producer is done,
it closes its queue front.
When all fronts to a queue have closed,
all threads holding a corresponding rear will receive notice
and should finish any remaning work,
close their other queue fronts, and exit.

A simple extension
is to enable multiple threads per stage.

```
p1.exec_n(3, f2, q1.rear(), q2.front());
```

# Implicit Queue Layer #

In this layer,
the pipeline is responsible for creating queues as needed
to connect the functions.
The pipline 'knows' whether or not a stage is producer/consumer
by overloading on the arguments to the function.

```
Pipeline p1;
p1.exec(f1).exec(f2).exec(f3);
// implicit p1.wait() in destructor of Pipeline
```

Because of the C++ rule on implicit destruction at the end of a full expression,
the above code could be written as

```
Pipeline().exec(f1).exec(f2).exec(f3);
```

# Operator Layer #

The `Pipeline` can support an operator syntax.

```
Pipeline() | f1 | f2 | f3;
```

# Adaptive Layer #

In the adaptive layer,
the pipeline is responsible for adapting the number of threads
to the work available and the idle processing power.

To enable this adaptation, the function must return between tasks.
The pipeline will ensure that at least one thread
is working in each stage.
An adaptive stage cannot be the first stage.
(Otherwise, "done with item" and "done with pipeline"
are indistinguishable.)

```
Pipeline().exec(f1).exec_adaptive(f2).exec(f3);
```

# Implicit Loop Layer #

In this layer, the `Pipeline` is responsible for creating the queue read loops.
The kinds of pipeline networks are much more constrained at this layer.

# Other Issues #

A `Pipeline` constructor should have a parameter
indicating the use of a thread pool.

# Examples #
Here are some discussion notes we've been using:
```
In order to end up with well formed pipelines, we need to diferentiate pipes
with unterminated sources and sinks.  A StartablePipeline has a source, a
EndablePipeline has a sink, and a RunablePipeline has both (and is actually
ready to run).

In general users won't see this complexity, as these would all derive from
Pipeline.


// everything is templated on IN and/or OUT types

// ***Basics
// Producers
static StartablePipeline Pipeline::Produce(OUT func());
static StartablePipeline Pipeline::Produce(void func(Queue::front));
static StartablePipeline Pipeline::Source(Queue::back);

// Filters don't require their own thread.
Pipeline Pipeline::Filter(OUT func(IN));
Pipeline Pipeline::Filter(func(IN, Queue::front));
StartablePipeline StartablePipeline::Filter(OUT func(IN));
StartablePipeline StartablePipeline::Filter(func(IN, Queue::front));

// Stages do require their own thread
Pipeline Pipeline::Stage(func(Queue::back, Queue::front));
Pipeline Pipeline::Stage(OUT func(Queue::back));
Pipeline Pipeline::Stage(Pipeline p);
EndablePipeline Pipeline::Stage(EndablePipeline p);
StartablePipeline StartablePipeline::Stage(func(Queue::back, Queue::front));
StartablePipeline StartablePipeline::Stage(OUT func(Queue::back));
StartablePipeline StartablePipeline::Stage(Pipeline p);
RunablePipeline StartablePipeline::Stage(EndablePipeline p);

// Consumers
EndablePipeline Pipeline::Consume(void func(IN));
EndablePipeline Pipeline::Sink(void func(Queue::front));
RunablePipeline StartablePipeline::Consume(void func(IN));
RunablePipeline StartablePipeline::Sink(void func(Queue::front));

// ***Parallel
// Split this traffic into n pipelines, and merge their outputs
Pipeline Pipeline::Parallel(Pipeline p, int n);
static StartablePipeline Pipeline::Parallel(StartablePipeline p, int n);
EndablePipeline Pipeline::Parallel(EndablePipeline p, int n);

// ***Alarm & Periodic
static StartablePipeline Pipeline::ProducePeriodic(OUT func(), period dt);
static StartablePipeline Pipeline::ProduceAt(OUT func(), time t);


// ***Examples:
// Read from a queue, filter, and consume
RunablePipeline z = Pipeline::Source(q).Filter(f).Consume(c);


// Produce, filter, and split across 3 pipes, then sink to a queue.
Pipeline p = Pipeline().Filter(f).Filter(g);
RunablePipeline y = Pipeline::Produce(k).Filter(l).Parallel(p, 3).Sink(r);

// Build a few pipe bits, and tie them together.
StartablePipeline s = Pipeline::Source(q).Filter(f);
Pipeline m = Pipeline().Stage(g).Filter(h);
EndablePipeline e = Pipeline().Filter(i).Sink(r);
RunablePipeline x = s.Parallel(m, 3).Parallel(e, 2);
// or GetThreadPool().run(s.Parallel(m, 3).Parallel(e, 2));

=================
Once you have a RunablePipeline, you can give it to a threadpool to make it go:
ThreadPool t;

t.run(p)
// or inline:
t.run(Pipeline::Source(q).Filter(f).Consume(c));

There could be optional paramters to run that allow you to specify additional
run constraints (threading strategies and whatnot).

The Pipeline class knows how many threads it requires to run, and has various
entry points where additional threads could run.
```
Compiler

Image Processing

# Older #

The pipieline execution framework is intended to allow executors to take control of execution flow of data dependent sequences of operations. This allows the user to specify functions which have output->input dependencies in sequence and to let the framework handle the scheduling of these tasks as data becomes available. This builds upon several existing components, including the source/sink interface (e.g. queues), the executor (e.g. thread pools), as well as enabling other execution structures (like the alarm service).

This framework is not intended to enable complex execution graphs, but instead is for linear sets of operations. The pipeline can be chained together with other pipelines, though, to create a sort of directed tree structure (even graphs are possible with connected pipelines).

# Details #

The pipeline structure builds primarily off of the executor concept. The executor allows for scheduling functions to execute in a delayed fashion by queuing up function objects for later execution. The primary example of the executor is the Thread Pool which creates threads of execution which pull operations off of the thread pool when new threads are available. This concept is extended by allowing the executor to take a data generator as the starting point for a pipeline.

For the purposes of the discussion, we will use the following naming conventions:
```
exec  -- executor used to schedule pipeline operations
q,r,s     -- blocking queue variables
f,g,h -- sequence of data-dependent functions (function objects)
```

A basic example of execution in the framework creates a simple list of operations which are data dependent and can each operate in parallel if data is available. This covers the simplest case where there is a simple data dependent set of operations.

```
exec.source(q).exec(f).exec(g).exec(h).end()
// Functionally equivalent to
for i in q
f(g(h(i)))
```

To support multiple outputs (or no outputs) on a given call, the caller can also provide a queue parameter as the output parameter. The call structure is the same as for the output-value version of the exec function, but the

```
std::function<void (char, queue<int>*)> f;
std::function<int (bool)> g;
exec.source(q).exec(f).consume(g);  // returns nothing (output of g is ignored)
exec.source(q).exec(f).exec(g).sink(q); // takes output of g and pushed into q
exec.source(q).exec(f).exec(g).collect(h);  //returns a std::future which returns the output of h when all the pipeline sources are closed
```

Executors can provide timed functions to start or control pipelines.

```
exec.run_at(t).produce(f).exec(g).sink(q);
exec.run_periodic(t, p).produce(f).exec(g).sink(q);
exec.run_periodic(t, p).produce(f).exec(g).consume(h);
exec.source(q).queue_periodic(p).exec(g)

exec.source(q).exec(queue_periodic(p)).consume(c)
```

## Examples ##

```

```

## Interface ##

```
class Executor {
 public:
  void add(std::function<void (void)> f);
};

class TaskMaster {
 public:
  TaskMaster(Executor* exec);
  ? run_after();
  ? run_periodic();

  template <class T, class U> friend class Stage<T, U>;
  template <class T> Stage* source(queue<T>* input);
};

template <class T, class U>
class Stage {
 public:
  // Builder functions for the next operation in the pipeline
  // What's the right C++11 way to handle owned vs. unowned function pointers?
  Stage* exec(std::function<T (U)>* func);
  Stage* exec(std::function<T (queue<U>*)>* func);

  // end function terminates the pipeline and returns the queue output of the last stage.
  // can return a null queue if the stage output type is void.
  queue<U>* end();

  // Functions to introspect into the pipeline state.
  bool is_stage_active();  // is the stage active (running or runnable)
  bool is_pipeline_active();  // is the stage or any subsequent stage active (running or runnable)

  // Functions to modify the behavior of the stage.
  enum Parallelism {
    SEQUENTIAL,  // Shared thread with previous stage
    SERIAL_ONLY,  // Single dedicated thread
    PREFER_SERIAL,  // Multhreadable but prefer single dedicated thread
    PARALLEL  // Prefer parallel with multiple threads.
  }
  void set_parallelism(Parallelism parallelism);
};

class ParallelStage : public Stage {
 public:
  void set_min_threads(int min_threads);
  void set_max_threads(int max_threads);
};

class SerialStage : public Stage {
 public:
};
```

### Ownership ###

The pipeline created by the TaskMaster is responsible for the creation of several components including pipeline stages, threads, and queues. The interface is defined such that anything which is used in the operation of the pipeline is owned by the pipeline. Unless otherwise stated, function objects also fall in this category as they are required to live as long as the pipeline can still execute. So internal stages, threads, and queues are owned by the pipeline and inputs/outputs are owned by the caller.

### Exception Handling ###

_TBD: How to handle exceptions which occur during execution of the pipeline_

## Implementation Details ##
There are a couple of competing desires when building a pipeline: to maximize the amount of flexibility in configuring the pipeline and in being able to optimize the level of parallelism. In an ideal world, there would just be queues between all stages and at least one unique thread for each stage. But in some cases, we would want to run subsequent stages in the same thread to minimize the overhead of inter-thread communication and context switching, which means supporting queued and non-queued communication as well as having the pipeline manage the flow of data between stages.

### Pipeline Creation ###
```

```

### Thread Creation ###
Thread creation algorithms is an open question. For stages which are forced to be serial, this isn't a question, but for parallel stages, there is a need to balance out processing the input and keeping the output busy while not taking up too many resources (dynamically adjusting to the time spent in a given stage).  A simple approach is to do something similar to the Java thread-pools: create threads when there is work to do, when threads go idle for too long, shut them down.

Note that the pipeline concept is intended to minimize deadlocking behavior _when used correctly_. This means not creating blocking dependencies between stages other than the implicit dependency on the data.
```

```

# Open Questions #

  * Alasidar: When queues are passed around, sometimes they don't need to be blocking queues? The classes can be templatized on the queue type, but for the parallel operations the blocking\_queue is required.
  * All: Do we want to handle creating a generic task-blocker which can take a predicate and a set of inputs and kick off later stages when the predicate is satisfied
  * All: What about a task distributor (like the shuffle operation in map-reduce), which would allow a single writer to redistribute work to thread-local queues to minimize the locking needed (since the reduce operations may work on fairly lightweight operations and adding blocking operations could be quite expensive).
  * All: do we want to create a map-reduce operation utilizing the pipeline concept? This could allow one to define complex data operations in terms of a pipeline of mapreduce operations. Someone want to spec this out?

# Links #
  * [NIPS'06 Mapreduce Paper](http://www.xmarks.com/site/www.cs.stanford.edu/people/ang/papers/nips06-mapreducemulticore.pdf)
  * [MSDN Task Parallelization Library](http://msdn.microsoft.com/en-us/library/dd537609.aspx)
  * [Java threadpool executor interface](http://download.oracle.com/javase/7/docs/api/java/util/concurrent/ThreadPoolExecutor.html)
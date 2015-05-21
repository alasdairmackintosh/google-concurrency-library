<h1>C++ Latches and Barriers</h1>

<p>
ISO/IEC JTC1 SC22 WG21 DRAFT N???? = 12-???? - 2012-03-30<br>
</p>

<p>
Alasdair Mackintosh, alasdair@google.com, alasdair.mackintosh@gmail.com<br>
</p>


<h2><a>Introduction</a></h2>

<p>
Certain idioms that are commonly used in concurrent programming are missing<br>
from the standard libraries. Although many of these these can be relatively<br>
straightforward to implement, we believe it is more efficient to have a<br>
standard version.<br>
<p>
</p>
In addition, although some idioms can be provided using<br>
mutexes, higher performance can often be obtained with atomic operations and<br>
lock-free algorithms which are more complex and prone to error.<br>
</p>
<p>
Other standard concurrency idioms may have difficult corner cases, and can be<br>
hard to implement correctly. For these<br>
</p>

<h2><a>Solution</a></h2>

<p>
We propose a set of commonly-used concurrency classes, some of which may be<br>
implemented using efficient lock-free algorithms where appropriate. This paper<br>
describes the <i>latch</i> and <i>barrier</i> classes.<br>
</p>
<p>
Latches are a thread co-ordination mechanism that allow one or more threads to<br>
block until an operation is completed. An individual latch is a single-use<br>
object; once the operation has been completed, it cannot be re-used.<br>
</p>
<p>
Barriers are a thread co-ordination mechanism that allow multiple threads to<br>
block until an operation is completed. Unlike a latch, a barrier is re-usable;<br>
once the operation has been completed, the threads can re-use the same<br>
barrier. It is thus useful for managing repeated tasks handled by multiple<br>
threads.<br>
</p>
<p>
A reference implementation of these two classes has been written.<br>
</p>

<h3><a>Latch Operations</a></h3>
A latch maintains an internal counter that is initialized when the latch is
created. One or more threads may block waiting until the counter is decremented
to 0.

<dl>
<dt><pre><code>constructor( size_t );</code></pre></dt>
<dd>
<p>
The parameter is the initial value of the internal counter.<br>
</p>
</dd>

<dt><pre><code>destructor( );</code></pre></dt>
<dd>
<p>
Destroys the latch. If the latch is destroyed while other threads are in<br>
<pre><code>wait()</code></pre>, or are invoking <pre><code>count_down()</code></pre>, the behaviour<br>
is undefined.<br>
</p>
</dd>

<dt><pre><code>void count_down( );</code></pre></dt>
<dd>
<p>
Decrements the internal count by 1, and returns. If the count reaches 0, any<br>
threads blocked in <pre><code>wait()</code></pre> will be released.<br>
</p>
<p>
Throws <pre><code>std::logic_error</code></pre> if the internal count is already 0.<br>
</p>
</dd>

<dt><pre><code>void wait( );</code></pre></dt>
<dd>
<p>
Blocks the calling thread until the internal count is decremented to 0 by one<br>
or more other threads calling <pre><code>count_down()</code></pre>. If the count is<br>
already 0, this is a no-op.<br>
</p>
</dd>

<dt><pre><code>void count_down_and_wait( );</code></pre></dt>
<dd>
<p>
Decrements the internal count by 1. If the resulting count is not 0, blocks the<br>
calling thread until the internal count is decremented to 0 by one or more<br>
other threads calling <pre><code>count_down()</code></pre>.<br>
</p>
</dd>
</dl>
<p>
There are no copy or assignment operations.<br>
</p>

<h4>Memory Ordering</h4>
All calls to ```
count_down()``` synchronize with any thread
returning from ```
wait()```.

<h3><a>Barrier Operations</a></h3>
<p>
A barrier maintains an internal thread counter that is initialized when the<br>
barrier is created. Threads may decrement the counter and then block waiting<br>
until the specified number of threads are blocked. All threads will then be<br>
woken up, and the barrier will reset. In addition, there is a mechanism to<br>
change the thread count value after the count reaches 0.<br>
</p>

<dl>
<dt><pre><code>constructor( size_t );</code></pre></dt>
<dd>
<p>
The parameter is the initial value of the internal thread counter.<br>
</p>
<p>
Throws <pre><code>std::invalid_argument</code></pre> if the specified count is 0.<br>
</p>
</dd>

<dt><pre><code>constructor( size_t, std::function&amp;lt;void()&amp;gt; );</code></pre></dt>
<dd>
<p>
The parameters are the initial value of the internal thread counter, and a<br>
function that will be invoked when the counter reaches 0.<br>
</p>
<p>
Throws <pre><code>std::invalid_argument</code></pre> if the specified count is 0.<br>
</p>
</dd>

<dt><pre><code>destructor( );</code></pre></dt>
<dd>
<p>
Destroys the barrier. If the barrier is destroyed while other threads are in<br>
<pre><code>count_down_and_wait()</code></pre>, the behaviour is undefined.<br>
</p>
</dd>

<dt><pre><code>void count_down_and_wait( );</code></pre></dt>
<dd>
<p>
Decrements the internal thread count by 1. If the resulting count is not 0,<br>
blocks the calling thread until the internal count is decremented to 0 by one<br>
or more other threads calling <pre><code>count_down_and_wait()</code></pre>.<br>
</p>
<p>
Before any threads are released, the completion function registered in the<br>
constructor will be invoked (if specified and non-NULL). Note that the<br>
completion function may be invoked in the context of one of the blocked<br>
threads.  When the completion function returns, the internal thread count will<br>
be reset to its initial value, and all blocked threads will be unblocked.<br>
<p>
Note that it is safe for a thread to re-enter <pre><code>count_down_and_wait()</code></pre>
immediately. It is not necessary to ensure that all blocked threads have exited<br>
<pre><code>count_down_and_wait()</code></pre> before one thread re-enters it.<br>
<br>
<br>
Unknown end tag for </dd><br>
<br>
<br>
<dt><pre><code>reset( size_t );</code></pre></dt>
<dd>
<p>
Resets the barrier with a new value for the initial thread count. This method may<br>
only be invoked when there are no other threads currently inside the<br>
<pre><code>count_down_and_wait()</code></pre> method. It may also be invoked from within<br>
the registered completion function.<br>
</p>
<p>
Once <pre><code>reset()</code></pre> is called, the barrier will automatically reset<br>
itself to the new thread count as soon as the internal count reaches 0 and all<br>
blocked threads are released.<br>
</p>
</dd>

<dt><pre><code>reset( std::function&amp;lt;void()&amp;gt; );</code></pre></dt>
<dd>
<p>
Resets the barrier with the new completion function. The next time the internal<br>
thread count reaches 0, this function will be invoked. This method may only be<br>
invoked when there are no other threads currently inside the<br>
<pre><code>count_down_and_wait()</code></pre> method. It may also be invoked from within<br>
the registered completion function.<br>
</p>
<p>
If NULL is passed in, no function will be invoked when the count reaches 0.<br>
</p>
</dd>

<br>
<br>
Unknown end tag for </dl><br>
<br>
<br>
<p>
There are no copy or assignment operations.<br>
</p>
<p>
Note that the barrier does not have separate <pre><code>count_down()</code></pre> and<br>
<pre><code>wait()</code></pre> methods. Every thread that counts down will then block until<br>
all threads have counted down. Hence only the<br>
<pre><code>count_down_and_wait()</code></pre> method is supported.<br>
</p>

<h4>Memory Ordering</h4>
For threads X and Y that call <pre><code>count_down_and_wait()</code></pre>, the<br>
call to <pre><code>count_down_and_wait()</code></pre> in X synchronizes with the return from<br>
<pre><code>count_down_and_wait()</code></pre> in Y.<br>
<br>
<br>
<h3><a>Sample Usage</a></h3>
Sample use cases for the latch include:<br>
<ul>
<li>
Setting multiple threads to perform a task, and then waiting until all threads<br>
have reached a common point.<br>
</li>
<li>
Creating multiple threads, which wait for a signal before advancing beyond a<br>
common point.<br>
</li>
</ul>
An example of the first use case would be as follows:<br>
<pre>

void DoWork(threadpool* pool) {<br>
latch completion_latch(NTASKS);<br>
for (int i = 0; i &lt; NTASKS; ++i) {<br>
pool-&gt;add_task([&] {<br>
// perform work<br>
...<br>
completion_latch.count_down();<br>
}));<br>
}<br>
// Block until work is done<br>
completion_latch.wait();<br>
}<br>
<br>
</pre>

An example of the second use case is shown below. We need to load data and then<br>
process it using a number of threads. Loading the data is I/O bound, whereas<br>
starting threads and creating data structures is CPU bound. By running these in<br>
parallel, throughput can be increased.<br>
<pre>
void DoWork() {<br>
latch start_latch(1);<br>
vector&lt;thread*&gt; workers;<br>
for (int i = 0; i &lt; NTHREADS; ++i) {<br>
workers.push_back(new thread([&] {<br>
// Initialize data structures. This is CPU bound.<br>
...<br>
start_latch.wait();<br>
// perform work<br>
...<br>
}));<br>
}<br>
// Load input data. This is I/O bound.<br>
...<br>
// Threads can now start processing<br>
start_latch.count_down();<br>
}<br>
</pre>
<p>
The barrier can be used to co-ordinate a set of threads carrying out a repeated<br>
task. The number of threads can be adjusted dynamically to respond to changing<br>
requirements.<br>
</p>
<p>
In the example below, a number of threads are performing a multi-stage<br>
task. Some tasks may require fewer steps thanothers, meaning that some threads<br>
may finish before others. We reduce the number of threads waiting on the<br>
barrier when this happens.<br>
</p>

<pre>

void DoWork() {<br>
Tasks& tasks;<br>
size_t initial_threads;<br>
atomic&lt;size_t&gt; current_threads(initial_threads)<br>
vector&lt;thread*&gt; workers;<br>
<br>
// Create a barrier, and set a lambda that will be invoked every time the<br>
// barrier counts down. If one or more active threads have completed,<br>
// reduce the number of threads.<br>
barrier task_barrier(n_threads);<br>
task_barrier.reset([&] {<br>
task_barrier.reset(current_threads);<br>
});<br>
<br>
for (int i = 0; i &lt; n_threads; ++i) {<br>
workers.push_back(new thread([&] {<br>
bool active = true;<br>
while(active) {<br>
Task task = tasks.get();<br>
// perform task<br>
...<br>
if (finished(task)) {<br>
current_threads--;<br>
active = false;<br>
}<br>
task_barrier.count_down_and_wait();<br>
}<br>
});<br>
}<br>
<br>
// Read each stage of the task until all stages are complete.<br>
while (!finished()) {<br>
GetNextStage(tasks);<br>
}<br>
}<br>
<br>
</pre>

<h2><a>Synopsis</a></h2>
<p>
The synopsis is as follows.<br>
</p>

<pre>

class latch {<br>
public:<br>
explicit latch(size_t count);<br>
~latch();<br>
<br>
void count_down();<br>
<br>
void wait();<br>
<br>
void count_down_and_wait();<br>
};<br>
<br>
class barrier {<br>
public:<br>
explicit barrier(size_t num_threads) throw (std::invalid_argument);<br>
explicit barrier(size_t num_threads, std::function&lt;void()&gt; f) throw (std::invalid_argument);<br>
~barrier();<br>
<br>
void count_down_and_wait();<br>
<br>
void reset(size_t num_threads);<br>
void reset(function f);<br>
};<br>
<br>
<br>
Unknown end tag for </code><br>
<br>
<br>
</pre>

<br>
<br>
Unknown end tag for </body><br>
<br>
<br>
<br>
<br>
Unknown end tag for </html><br>
<br>

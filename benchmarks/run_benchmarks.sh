#!/bin/bash
# Fern Language Benchmarks - Compare with C and Python
set -e
cd "$(dirname "$0")/.."

echo "╭──────────────────────────────────────────────────────────────╮"
echo "│           Fern vs C vs Python Benchmarks                     │"
echo "╰──────────────────────────────────────────────────────────────╯"
echo ""

# Ensure fern is built
if [ ! -f bin/fern ]; then
    echo "Building Fern compiler..."
    make debug > /dev/null 2>&1
fi

RUNS=5

# Compile all benchmarks
echo "Compiling benchmarks..."
clang -O2 -o /tmp/fib_c benchmarks/fib.c
./bin/fern build benchmarks/fib_fern.fn 2>/dev/null
clang -O2 -o /tmp/startup_c benchmarks/startup.c
./bin/fern build benchmarks/startup_fern.fn 2>/dev/null
echo ""

# ─────────────────────────────────────────────────────────────────
echo "╭─────────────────────────────────────────────────────────────╮"
echo "│  Fibonacci(35) - Recursive Tree (CPU-bound)                 │"
echo "├─────────────────────────────────────────────────────────────┤"
echo "│  Each program computes fib(35) = 9,227,465 via recursion.  │"
echo "│  This tests function call overhead and basic arithmetic.    │"
echo "╰─────────────────────────────────────────────────────────────╯"
echo ""

# Warmup
/tmp/fib_c > /dev/null
./fib_fern > /dev/null
python3 benchmarks/fib.py > /dev/null

echo "Running $RUNS iterations each..."
echo ""

# C benchmark
c_total=0
echo -n "  C:      "
for i in $(seq 1 $RUNS); do
    t=$(/usr/bin/time -p /tmp/fib_c 2>&1 | grep "^real" | awk '{print $2}')
    c_total=$(echo "$c_total + $t" | bc)
    echo -n "${t}s "
done
c_avg=$(echo "scale=2; $c_total / $RUNS" | bc)
echo "→ avg: ${c_avg}s"

# Fern benchmark
fern_total=0
echo -n "  Fern:   "
for i in $(seq 1 $RUNS); do
    t=$(/usr/bin/time -p ./fib_fern 2>&1 | grep "^real" | awk '{print $2}')
    fern_total=$(echo "$fern_total + $t" | bc)
    echo -n "${t}s "
done
fern_avg=$(echo "scale=2; $fern_total / $RUNS" | bc)
echo "→ avg: ${fern_avg}s"

# Python benchmark
py_total=0
echo -n "  Python: "
for i in $(seq 1 $RUNS); do
    t=$(/usr/bin/time -p python3 benchmarks/fib.py 2>&1 | grep "^real" | awk '{print $2}')
    py_total=$(echo "$py_total + $t" | bc)
    echo -n "${t}s "
done
py_avg=$(echo "scale=2; $py_total / $RUNS" | bc)
echo "→ avg: ${py_avg}s"

echo ""
echo "  Results:"
fern_vs_c=$(echo "scale=1; $fern_avg / $c_avg" | bc 2>/dev/null || echo "N/A")
py_vs_c=$(echo "scale=1; $py_avg / $c_avg" | bc 2>/dev/null || echo "N/A")
py_vs_fern=$(echo "scale=1; $py_avg / $fern_avg" | bc 2>/dev/null || echo "N/A")
echo "  • Fern is ${fern_vs_c}x C speed"
echo "  • Python is ${py_vs_c}x C speed"
echo "  • Fern is ${py_vs_fern}x faster than Python"
echo ""

# ─────────────────────────────────────────────────────────────────
echo "╭─────────────────────────────────────────────────────────────╮"
echo "│  Startup Time - Minimal Program                             │"
echo "├─────────────────────────────────────────────────────────────┤"
echo "│  Measures process creation and minimal execution time.      │"
echo "│  Important for CLI tools and short-running scripts.         │"
echo "╰─────────────────────────────────────────────────────────────╯"
echo ""

echo "Running 20 iterations each..."
echo ""

# C startup
c_start=$(python3 -c "import time; start=time.time(); import subprocess; [subprocess.run(['/tmp/startup_c'], capture_output=True) for _ in range(20)]; print(f'{(time.time()-start)/20*1000:.1f}')")
echo "  C:      ${c_start}ms per invocation"

# Fern startup
fern_start=$(python3 -c "import time; start=time.time(); import subprocess; [subprocess.run(['./startup_fern'], capture_output=True) for _ in range(20)]; print(f'{(time.time()-start)/20*1000:.1f}')")
echo "  Fern:   ${fern_start}ms per invocation"

# Python startup
py_start=$(python3 -c "import time; start=time.time(); import subprocess; [subprocess.run(['python3', 'benchmarks/startup.py'], capture_output=True) for _ in range(20)]; print(f'{(time.time()-start)/20*1000:.1f}')")
echo "  Python: ${py_start}ms per invocation"

echo ""
echo "  Results:"
fern_vs_py=$(echo "scale=1; $py_start / $fern_start" | bc 2>/dev/null || echo "N/A")
echo "  • Fern starts ${fern_vs_py}x faster than Python"
echo ""

# ─────────────────────────────────────────────────────────────────
echo "╭─────────────────────────────────────────────────────────────╮"
echo "│  Binary Size Comparison                                     │"
echo "╰─────────────────────────────────────────────────────────────╯"
echo ""

c_size=$(ls -l /tmp/fib_c | awk '{print $5}')
fern_size=$(ls -l ./fib_fern | awk '{print $5}')

echo "  C:      $(echo "scale=1; $c_size / 1024" | bc)KB"
echo "  Fern:   $(echo "scale=1; $fern_size / 1024" | bc)KB"
echo ""

# Cleanup
rm -f fib_fern sum_fern startup_fern

echo "╭─────────────────────────────────────────────────────────────╮"
echo "│  Summary                                                    │"
echo "├─────────────────────────────────────────────────────────────┤"
echo "│  Fern compiles to native code via QBE backend.              │"
echo "│  Performance is close to optimized C, much faster than      │"
echo "│  interpreted Python. Single binary deployment like Go.      │"
echo "╰─────────────────────────────────────────────────────────────╯"

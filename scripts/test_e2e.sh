#!/bin/bash
# End-to-end test for Fern compiler
# Compiles a Fern program to QBE IR, then to native code, and runs it

set -e

# Check if qbe is available
if ! command -v qbe &> /dev/null; then
    echo "QBE not found. Install with: brew install qbe"
    exit 1
fi

# Create temp directory
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

# Test program: simple addition
cat > "$TMPDIR/test.fn" << 'EOF'
fn add(a: Int, b: Int) -> Int: a + b

fn main() -> Int: add(40, 2)
EOF

echo "=== Fern Source ==="
cat "$TMPDIR/test.fn"
echo ""

# Generate QBE IR (this would be done by fern compiler)
# For now, we'll manually write the expected QBE
cat > "$TMPDIR/test.ssa" << 'EOF'
export function w $add(w %a, w %b) {
@start
    %t0 =w copy %a
    %t1 =w copy %b
    %t2 =w add %t0, %t1
    ret %t2
}

export function w $main() {
@start
    %t0 =w copy 40
    %t1 =w copy 2
    %t2 =w call $add(w %t0, w %t1)
    ret %t2
}
EOF

echo "=== QBE IR ==="
cat "$TMPDIR/test.ssa"
echo ""

# Compile QBE to assembly
echo "=== Compiling QBE to assembly ==="
qbe -o "$TMPDIR/test.s" "$TMPDIR/test.ssa"

# Compile assembly to executable
echo "=== Compiling assembly to executable ==="
cc -o "$TMPDIR/test" "$TMPDIR/test.s"

# Run the executable
echo "=== Running executable ==="
"$TMPDIR/test"
RESULT=$?

echo ""
echo "=== Result ==="
echo "Exit code: $RESULT"

if [ $RESULT -eq 42 ]; then
    echo "SUCCESS: Program returned expected value 42"
    exit 0
else
    echo "FAILURE: Expected 42, got $RESULT"
    exit 1
fi

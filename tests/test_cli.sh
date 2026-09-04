#!/bin/bash
# test_cli.sh — Tests for new wlite CLI features
#
# Tests:
# - wlite migrate (create table)
# - wlite migrate --force (skip prompts)
# - wlite diff --json (detailed JSON output)
# - wlite plan --json (detailed JSON output)
# - wlite compile (binary .wlitem output)

set -e

WLITE="./wlite"
export LD_LIBRARY_PATH=../libwlite
PASS=0
FAIL=0

pass() { PASS=$((PASS+1)); echo "  PASS: $1"; }
fail() { FAIL=$((FAIL+1)); echo "  FAIL: $1"; }

echo "wlite CLI tests"
echo ""

# Setup temp directory
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

# Create a test model
cat > "$TMPDIR/app.wlite" << 'EOF'
model_config {
    name "test_app"
    version 1
}

model User {
    table "users"

    field id integer {
        primary_key
        autoincrement
    }

    field name text {
        not_null
    }

    field email text
}
EOF

# ── Test: wlite migrate creates table ───────────────────────────────

echo "Test: wlite migrate creates table"
$WLITE migrate "$TMPDIR/test.db" "$TMPDIR/app.wlite" --force 2>/dev/null
if $WLITE query "$TMPDIR/test.db" "SELECT name FROM sqlite_master WHERE type='table' AND name='users'" 2>/dev/null | grep -q users; then
    pass "table created"
else
    fail "table not created"
fi

# ── Test: wlite migrate is idempotent ───────────────────────────────

echo "Test: wlite migrate is idempotent"
$WLITE migrate "$TMPDIR/test.db" "$TMPDIR/app.wlite" --force 2>/dev/null
$WLITE migrate "$TMPDIR/test.db" "$TMPDIR/app.wlite" --force 2>/dev/null
if [ $? -eq 0 ]; then
    pass "second migrate succeeded"
else
    fail "second migrate failed"
fi

# ── Test: wlite diff --json produces valid JSON ─────────────────────

echo "Test: wlite diff --json"
$WLITE diff "$TMPDIR/test.db" "$TMPDIR/app.wlite" --json 2>/dev/null | grep -q '"change_count"'
if [ $? -eq 0 ]; then
    pass "diff --json has change_count"
else
    fail "diff --json missing change_count"
fi

# ── Test: wlite diff --json with changes ────────────────────────────

echo "Test: wlite diff --json with changes"
cat > "$TMPDIR/app2.wlite" << 'EOF'
model_config {
    name "test_app"
    version 1
}

model User {
    table "users"

    field id integer {
        primary_key
        autoincrement
    }

    field name text {
        not_null
    }

    field email text

    field bio text
}
EOF
DIFF_OUT=$($WLITE diff "$TMPDIR/test.db" "$TMPDIR/app2.wlite" --json 2>/dev/null || true)
if echo "$DIFF_OUT" | grep -q '"changes"'; then
    pass "diff --json with changes"
else
    fail "diff --json missing changes array"
fi

# ── Test: wlite plan --json produces valid JSON ─────────────────────

echo "Test: wlite plan --json"
$WLITE plan "$TMPDIR/test.db" "$TMPDIR/app2.wlite" --json 2>/dev/null | grep -q '"step_count"'
if [ $? -eq 0 ]; then
    pass "plan --json has step_count"
else
    fail "plan --json missing step_count"
fi

# ── Test: wlite plan --json has steps array ─────────────────────────

echo "Test: wlite plan --json has steps"
PLAN_OUT=$($WLITE plan "$TMPDIR/test.db" "$TMPDIR/app2.wlite" --json 2>/dev/null || true)
if echo "$PLAN_OUT" | grep -q '"steps"'; then
    pass "plan --json has steps"
else
    fail "plan --json missing steps"
fi

# ── Test: wlite compile produces binary file ────────────────────────

echo "Test: wlite compile"
$WLITE compile "$TMPDIR/app.wlite" -o "$TMPDIR/schema.wlitem" 2>/dev/null
if [ -f "$TMPDIR/schema.wlitem" ]; then
    # Check it's not JSON (should be binary)
    if head -c 4 "$TMPDIR/schema.wlitem" | grep -q '{'; then
        fail "compile produced JSON instead of binary"
    else
        pass "compile produced binary .wlitem"
    fi
else
    fail "compile did not produce output file"
fi

# ── Test: wlite migrate --force skips prompts ───────────────────────

echo "Test: wlite migrate --force"
rm -f "$TMPDIR/force.db"
$WLITE migrate "$TMPDIR/force.db" "$TMPDIR/app.wlite" --force 2>/dev/null
if [ $? -eq 0 ]; then
    pass "migrate --force succeeded"
else
    fail "migrate --force failed"
fi

# ── Test: wlite snapshot works ──────────────────────────────────────

echo "Test: wlite snapshot"
SNAP=$($WLITE snapshot "$TMPDIR/test.db" 2>/dev/null)
if [ -n "$SNAP" ]; then
    pass "snapshot produces output"
else
    fail "snapshot produced no output"
fi

# ── Test: wlite hash works ──────────────────────────────────────────

echo "Test: wlite hash"
HASH=$($WLITE hash "$TMPDIR/test.db" 2>/dev/null)
if [ -n "$HASH" ] && [ "$HASH" != "?" ]; then
    pass "hash produces value"
else
    fail "hash produced empty or ?"
fi

# ── Test: wlite status works ────────────────────────────────────────

echo "Test: wlite status"
STATUS=$($WLITE status "$TMPDIR/test.db" 2>/dev/null)
if echo "$STATUS" | grep -q "Schema hash:"; then
    pass "status shows schema hash"
else
    fail "status missing schema hash"
fi

# ── Summary ─────────────────────────────────────────────────────────

echo ""
echo "$PASS passed, $FAIL failed"
exit $FAIL

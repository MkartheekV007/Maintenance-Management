#!/usr/bin/env bash
# End-to-end checks against a throw-away copy of ./data
set -u
cd "$(dirname "$0")/.."
BIN=${BIN:-./maintenance}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cp data/*.csv "$TMP"/
export MAINT_DATA_DIR="$TMP"

fail=0
check() { # name, output, expected text
  if grep -qF -- "$3" <<<"$2"; then echo "PASS  $1"; else echo "FAIL  $1  (expected: $3)"; echo "$2"; fail=1; fi
}

out=$(printf '3\n101\nomsrisairam\n5\n' | $BIN)
check "admin login works"            "$out" "Login Successful! Welcome Chhayank"

out=$(printf '3\n101\nwrong\n' | $BIN)
check "wrong password rejected"      "$out" "Invalid Login!"

out=$(printf '3\n108\nomsrisairam\n1\nMic\n5\n1\nmic \n5\n5\n' | $BIN)
check "stock add merges duplicates"  "$out" "now has 10"

out=$(printf '2\ntester\n1\n29-09-2026\nmic\n3\n' | $BIN)
check "SRD request sees stock"       "$out" "is Available (Qty: 10)"

out=$(printf '1\n123\nstudent123\n1\n29-09-2026\n2\nRoom A9\n1\n29-09-2026\n3\nRoom B1\n3\n' | $BIN)
check "duty member from duties.csv"  "$out" "Assigned Duty Member: Preetam"
check "first complaint gets ID 101"  "$out" "Your Repair ID is: 101"
check "second complaint gets ID 102" "$out" "Your Repair ID is: 102"

out=$(printf '1\n123\nstudent123\n2\n101\n3\n' | $BIN)
check "status: pending"              "$out" "Status: PENDING"

out=$(printf '3\n108\nomsrisairam\n3\n5\n' | $BIN)
check "admin sees first complaint"   "$out" "ID: 101"

out=$(printf '3\n108\nomsrisairam\n4\n101\nnone\nn\n5\n' | $BIN)
check "complete repair"              "$out" "Repair Completed Successfully!"

out=$(printf '1\n123\nstudent123\n2\n101\n3\n' | $BIN)
check "status: resolved"             "$out" "Status: RESOLVED"

out=$(printf '1\n123\nstudent123\n1\n31-02-2026\n' | $BIN)
check "bad date rejected"            "$out" "Invalid date"

[ $fail -eq 0 ] && echo "All smoke tests passed." || { echo "Some tests FAILED."; exit 1; }

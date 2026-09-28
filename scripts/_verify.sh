# Sourced by the fetch scripts. verify_sha256 FILE EXPECTED: abort if the file's SHA-256 differs.
# Hashes were pinned from the upstream GitHub release files; a mismatch means the file changed.
verify_sha256() {
  local actual
  if command -v sha256sum >/dev/null; then actual="$(sha256sum "$1" | cut -d' ' -f1)"
  else actual="$(shasum -a 256 "$1" | cut -d' ' -f1)"; fi
  if [ "$actual" != "$2" ]; then
    echo "SHA-256 mismatch for $1" >&2
    echo "  expected $2" >&2
    echo "  actual   $actual" >&2
    rm -f "$1"
    exit 1
  fi
}

#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
make --no-print-directory -C "$project_dir" ohmydeparture
exec "$project_dir/ohmydeparture" "$@"

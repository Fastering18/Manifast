#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
[[ -f .githooks/pre-push ]] || { echo "Missing .githooks/pre-push"; exit 1; }
chmod +x .githooks/pre-push scripts/check-before-push.sh || true
git config core.hooksPath .githooks
echo "Installed git hooks: core.hooksPath=.githooks"
echo "Pre-push will run scripts/check-before-push.sh (tests + WASM)."
echo "Emergency bypass: SKIP_PUSH_CHECKS=1 git push"

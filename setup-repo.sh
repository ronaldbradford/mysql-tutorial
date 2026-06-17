#!/usr/bin/env bash
#
# Initialize this directory as a git repository and push it to GitHub.
#
# Prerequisites:
#   - git
#   - GitHub CLI (gh), authenticated:  gh auth login
#
# Usage:
#   ./setup-repo.sh [github-account]
#
# If github-account is omitted, the repo is created under your default
# gh account. Example accounts you use: ronaldbradford
#
set -euo pipefail

REPO_NAME="mysql-tutorial"
ACCOUNT="${1:-}"
VISIBILITY="--public"

cd "$(dirname "$0")"

if [ -d .git ]; then
  echo "Git repository already initialized here."
else
  git init -b main
  git add .
  git commit -m "Initial commit: uuidv() loadable function (component + UDF)"
fi

if ! command -v gh >/dev/null 2>&1; then
  echo "GitHub CLI (gh) not found. Install it, run 'gh auth login', then re-run."
  echo "The local repository is ready; you can push manually with:"
  echo "  git remote add origin git@github.com:<account>/${REPO_NAME}.git"
  echo "  git push -u origin main"
  exit 0
fi

if [ -n "$ACCOUNT" ]; then
  REPO_PATH="${ACCOUNT}/${REPO_NAME}"
else
  REPO_PATH="${REPO_NAME}"
fi

gh repo create "$REPO_PATH" $VISIBILITY \
  --source=. --remote=origin --push \
  --description="MySQL extension examples, starting with a uuidv() loadable function"

echo "Done. Repository pushed to GitHub: ${REPO_PATH}"

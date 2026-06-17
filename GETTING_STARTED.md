# Getting this onto your machine and into Claude Code

This `mysql-tutorial/` directory is a ready-to-push Git repository. Here is how
to land it on `phoenix` and continue working on it in Claude Code.

## 1. Download and place it

Download the `mysql-tutorial/` directory from this chat and move it where you
keep your projects, for example:

```bash
mv ~/Downloads/mysql-tutorial ~/projects/mysql-tutorial
cd ~/projects/mysql-tutorial
```

## 2. Create the GitHub repository and push

The included script initializes git and creates the GitHub repo using the
`gh` CLI. Make sure you are authenticated first (`gh auth login`), then:

```bash
# Default account:
./setup-repo.sh

# Or under a specific account, e.g. ronaldbradford:
./setup-repo.sh ronaldbradford
```

If you do not use `gh`, the script still initializes the local repo and prints
the manual `git remote add` / `git push` commands.

## 3. Open it in Claude Code

```bash
cd ~/projects/mysql-tutorial
claude
```

`CLAUDE.md` is already present, so Claude Code will pick up the repo
conventions (no trailing whitespace, `#!/usr/bin/env python`, MySQL server
build conventions) automatically.

## Why this two-step approach

This chat runs in an isolated sandbox that cannot reach `phoenix`, your GitHub
account, or your local `claude` CLI. So the repository is assembled here and
the final create/push happens on your machine, where your credentials and the
MySQL source tree live.

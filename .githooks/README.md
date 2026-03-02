# Git Hooks

Install hooks for this repository:

```bash
./tools/install_git_hooks.sh
```

Available hooks:

- `pre-commit`: dependency boundary checks + build (optional smoke checks via `RUN_SMOKE_CHECKS=1`)
- `pre-push`: CI checks (optional integration checks via `RUN_INTEGRATION_CHECKS=1`)
- `commit-msg`: enforces `type: summary` format
- `post-merge`: reminds about hooks setup and runs quick dependency check

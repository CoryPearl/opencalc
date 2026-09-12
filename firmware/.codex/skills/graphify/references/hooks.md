# graphify reference: commit hook and Codex integration

Load this when the user asks to install the post-commit hook or wire graphify into a project's Codex instructions.

## For git commit hook

Install a post-commit hook that auto-rebuilds the graph after every commit. No background process needed - triggers once per commit, works with any editor.

```bash
graphify hook install    # install
graphify hook uninstall  # remove
graphify hook status     # check
```

After every `git commit`, the hook detects which code files changed (via `git diff HEAD~1`), re-runs AST extraction on those files, and rebuilds `graph.json` and `GRAPH_REPORT.md`. Doc/image changes are ignored by the hook - run `/graphify --update` manually for those.

If a post-commit hook already exists, graphify appends to it rather than replacing it.

---

## For Codex integration

Install or refresh the Graphify skill for Codex:

```bash
graphify install --platform codex
```

Keep project-specific instructions in `.codex/README.md` and the portable skill in
`.codex/skills/graphify/SKILL.md`. The skill instructs the agent to check the graph
before answering codebase questions and rebuild it after code changes.

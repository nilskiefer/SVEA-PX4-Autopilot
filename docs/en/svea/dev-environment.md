# Dev Environment (VS Code + Devcontainer)

## 1) Prerequisites

- VS Code
- Docker Engine running
- VS Code extension: `Dev Containers` (`ms-vscode-remote.remote-containers`)
- USB access to board/probe from host

## 2) Open the Repository Correctly

1. Open the repo folder in VS Code.
2. If prompted, click **Trust** for this repository/workspace.
3. Run `Dev Containers: Reopen in Container`.

## 3) Important Linux Popups (Do Not Skip)

On Linux, VS Code may show multiple trust/safety popups during startup and when nested repos/submodules are scanned.

You must:

1. Wait for all prompts to appear.
2. Accept/trust each repository prompt (including “unsafe repository” style prompts).
3. Ensure all PX4 sub-repositories are trusted before continuing.

If you skip these prompts, extension tasks and tooling can partially fail.

## 4) Verify Environment Inside Container

Open terminal in VS Code (inside container) and check:

```bash
pwd
git status
```

You should be in the SVEA PX4 repo and have normal git access.

## 5) Continue to Build/Flash

After container/trust setup is complete:

- [Quick Bringup](quick-bringup.md)
- [Build and Flash](build-and-flash.md)

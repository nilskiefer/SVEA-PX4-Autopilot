# Set up the build environment {#dev-environment-vs-code-devcontainer}

Use the VS Code devcontainer to build SVEA firmware from source. To install a published image, go straight to the [browser flasher](flash.md).

## Prerequisites {#_1-prerequisites}

- VS Code with the **Dev Containers** extension (`ms-vscode-remote.remote-containers`).
- A running Docker Engine.
- A local checkout of the SVEA firmware repository.

## Open the devcontainer {#_2-open-the-repository-correctly}

1. Open the repository folder in VS Code and approve workspace trust for your checkout.
2. Run **Dev Containers: Reopen in Container** from the command palette.

### Linux trust prompts {#_3-important-linux-popups-do-not-skip}

Wait for VS Code to finish scanning submodules. Approve the trust prompts for the PX4 sub-repositories in your checkout. Extension tasks can fail if those prompts are left open.

## Check the container {#_4-verify-environment-inside-container}

Open a terminal in VS Code after the container starts.

```bash
pwd
git status
```

Once `git status` works from the firmware checkout, [build the Clicker4 image](build-and-flash.md#build).

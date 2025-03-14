# Maintainers

This document describes some instructions for maintainers. Other contributors and users need not be concerned with this material.

### GitHub instructions

When setting up the repository on GitHub, configure the following settings:

- Under `Secrets`:
  - Under `Actions`, add the following repository secrets with appropriate values:
    - `DOCKER_PASSWORD`
  - Under `Dependabot`, add the `DOCKER_PASSWORD` repository secret with an appropriate value (e.g., that of the corresponding secret above).
- Under `Branches`, add a branch protection rule for the `main` branch.
  - Enable `Require status checks to pass before merging`.
    - Enable `Require branches to be up to date before merging`.
    - Add the following status checks:
      - `Build for Linux`
  - Enable `Include administrators`.
- Under `Options`, enable `Automatically delete head branches`.

The GitHub workflow will fail initially because the jobs which test the installer script will not find any release to download. You'll need to bootstrap a release by temporarily removing those jobs or changing them to no-ops. Be aware that the `create-release` job is configured to only run on the `main` branch, so you may also need to temporarily change that depending on which branch you're working on.
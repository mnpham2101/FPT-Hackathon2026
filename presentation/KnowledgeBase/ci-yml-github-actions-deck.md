---
title: CI Workflows, YAML and GitHub Actions
description: What a CI workflow file is, the Docker actions it performs, and the tagging limitation this project lives with
deck: Knowledge Base · CI workflows, YAML and GitHub Actions
---

<!-- _class: lead -->
![bg](../assets/bg-title-city.jpg)

# CI Workflows, YAML and GitHub Actions

## What the `.yml` files do, and what a tag really promises

**Knowledge Base**

Cooperative Vehicle Awareness · Milestone 1

---

# Table of contents

1. **Continuous integration** — what GitHub Actions is for, and the words it uses
2. **Docker** — images, containers, registries, and the two verbs that matter
3. **The workflow file** — its shape, its syntax, and where the real documentation lives
4. **Image identity** — name, tag and digest, and which of them a build chooses
5. **Limitation in this project** — one push, two builds, one tag

---

<!-- _class: lead -->
![bg](../assets/bg-navy-motif.png)

# 01 · Continuous integration

---

# The role of continuous integration

Continuous integration is the practice of building and testing every change automatically, on a machine that is not the author's laptop.

- **It answers one question:** does this change still build, still pass its tests, and still produce a usable artifact?
- **It runs on a clean machine every time**, so "it works on my machine" stops being evidence.
- **It runs unattended**, on a trigger — nobody has to remember to start it.

> **GitHub Actions** is GitHub's built-in system for this. The instructions live in the repository itself, as files under `.github/workflows/`, so the build procedure is versioned with the code it builds.

---

# The vocabulary

The terms below are GitHub's, not this project's. They appear throughout the workflow files and in every error message.

| Term | What it is |
|---|---|
| **Workflow** | One `.yml` file under `.github/workflows/`. A complete automated procedure |
| **Event** | What starts a workflow — a `push`, a `pull_request`, a schedule |
| **Run** | One execution of a workflow, caused by one event |
| **Job** | A named group of steps inside a workflow. Jobs run in parallel by default |
| **Step** | A single instruction in a job — either a shell command or a reusable action |
| **Action** | A packaged, reusable step, referenced with `uses:` |
| **Runner** | The virtual machine the job executes on |

> This project also says **lane** for a job — a local nickname, not a GitHub term.

---

# The authoritative documentation

Nothing in this deck replaces GitHub's own reference. Use these when writing or debugging a workflow.

| Source | What to use it for |
|---|---|
| [docs.github.com/en/actions](https://docs.github.com/en/actions) | The starting point — concepts, guides, examples |
| [Workflow syntax reference](https://docs.github.com/en/actions/writing-workflows/workflow-syntax-for-github-actions) | Every key that is legal in a workflow file, and what it means |
| [Events that trigger workflows](https://docs.github.com/en/actions/writing-workflows/choosing-when-your-workflow-runs) | The full list of triggers and their filters |
| [docs.docker.com](https://docs.docker.com/) | Docker itself — build, push, pull, run |

---

<!-- _class: lead -->
![bg](../assets/bg-navy-motif.png)

# 02 · Docker

---

# Images and containers

Docker packages an application together with everything it needs to run, so the same bundle behaves identically on any machine.

- **Image** — a frozen, read-only bundle: the filesystem, the libraries, the program, and the command to start it. Built once, never modified.
- **Container** — a running instance of an image. Starting the same image twice gives two containers.
- **Registry** — a server that stores images so other machines can fetch them. This project uses **Zot**, at `registry.hackathon-2.carsky.io`.

An image is to a container what a program on disk is to a process in memory.

---

# The four actions a build workflow performs

Plain terms first, Docker's own terms in brackets. These four are what every image job in this repository does.

| # | Plain term | Docker term | What happens |
|---|---|---|---|
| 1 | **Build the image** | `docker build` | Turn source code into an image on the runner |
| 2 | **Upload the image** | **push** — `docker push` | Send the image from the runner to the registry |
| 3 | **Check the registry** | HTTP request | Ask the registry whether the image is really there |
| 4 | **Download the image** | **pull** — `docker pull` | Fetch it back from the registry and start it |

> Steps 3 and 4 exist because a successful build only proves the code compiles. Only fetching the image back proves the registry holds something another machine can actually run.

---

<!-- _class: lead -->
![bg](../assets/bg-navy-motif.png)

# 03 · The workflow file

---

# Anatomy of a workflow file

YAML is an indentation-based format: nesting is expressed by spaces, never by braces or tabs. Four top-level keys carry a workflow.

```yaml
name: phase1-ci                 # what the workflow is called

on:                             # WHEN it runs
  push:
  pull_request:
    branches: [main]

jobs:                           # WHAT it runs
  v2x-ecu-image:                # <- a job, named by its author
    runs-on: ubuntu-latest      # which runner
    steps:                      # <- the ordered instructions
      - uses: actions/checkout@v4     # a reusable action
      - name: Build the image         # a shell command
        run: docker build -t my-image .
```

- **`run:`** executes a shell command. **`uses:`** calls a packaged action someone else wrote.
- Full key-by-key reference: [Workflow syntax for GitHub Actions](https://docs.github.com/en/actions/writing-workflows/workflow-syntax-for-github-actions).

---

# Upload (push) — the code

Build the image and send it to the registry. `--push` makes buildx upload the result instead of leaving it on the runner.

```yaml
- name: Build and upload the image
  run: |
    docker buildx build \
      --platform linux/arm64 \
      -t registry.hackathon-2.carsky.io/m1-v2x-ecu:latest \
      --push \
      --metadata-file "$RUNNER_TEMP/push-metadata.json" \
      V2X_ECU/

    # buildx reports the digest of exactly what it uploaded
    DIGEST=$(jq -r '."containerimage.digest"' "$RUNNER_TEMP/push-metadata.json")
    echo "[PUSH] digest of this build: $DIGEST"
```

- **`-t`** names the image being uploaded — registry host, image name and tag in one string.
- **`--metadata-file`** is how the job learns the digest; the build chooses the digest, not the author.

---

# Download (pull) — the code

Fetch the image back out of the registry and start a container from it, on a runner that has just been wiped of the local build.

```yaml
- name: Download the image and start it
  run: |
    docker login registry.hackathon-2.carsky.io -u "$USER" --password-stdin <<< "$KEY"

    docker pull --platform linux/arm64 \
      registry.hackathon-2.carsky.io/m1-v2x-ecu:latest

    # start it and confirm the application comes up
    docker run --rm --platform linux/arm64 \
      registry.hackathon-2.carsky.io/m1-v2x-ecu:latest
```

- **A pull needs credentials** exactly as a push does — the registry is private.
- This is the same operation a CarSky node performs when it deploys.

---

# An image's three identifiers

One image, three ways to refer to it. Two are chosen by the author; the third is not.

```text
  registry.hackathon-2.carsky.io / m1-v2x-ecu : latest
  └────────── registry host ─────┘ └── name ──┘ └tag─┘

  registry.hackathon-2.carsky.io/m1-v2x-ecu@sha256:149c5433198d2e…
                                            └──── digest (hash) ───┘
```

| Identifier | Chosen by | Can it change? |
|---|---|---|
| **Name** | The author, in `-t` | Fixed for the life of the image |
| **Tag** | The author, in `-t` after the `:` | **Movable** — can be repointed at a different image at any time |
| **Digest** | Computed by the build, from the image's own bytes | Never — a different image always has a different digest |

> In `-t registry.…/m1-v2x-ecu:latest`, everything before the last `:` is the name, and `latest` is the tag. A tag is a label, not an identity.

---

<!-- _class: lead -->
![bg](../assets/bg-navy-motif.png)

# 04 · Limitation in this project

---

# Two builds from one git push

Every workflow file here subscribes to two events. One `git push` to a branch with an open pull request satisfies both.

```yaml
on:
  push:                      # fires on a push to any branch
  pull_request:
    branches: [main]         # ALSO fires, for a branch with an open PR
```

- **GitHub starts two runs**, on the same commit, at the same time. Each executes the whole file, including the image jobs.
- **Each run builds its own image.** Container builds are not byte-identical — timestamps and layer ordering differ — so the two images have **two different digests**.
- **Both upload under the same name and tag**, `m1-v2x-ecu:latest`, because the tag is written into `-t` as a constant.

> A tag points at one image. The second upload to finish wins it; the first run's image stays in the registry with nothing pointing at it.

---

# The tag a node loads

A CarSky node does not know about builds, commits or pull requests. It is given one string.

- **A node's `image` field takes a tag** — `registry.hackathon-2.carsky.io/m1-v2x-ecu:latest`. Every blueprint and node guide in this repository uses that form.
- **The node pulls whatever the tag points at**, at the moment it deploys. It has no way to ask which build produced it.
- **`:latest` may hold either run's image** — the push run's or the pull-request run's, decided by which finished second.

> The image running on a node can therefore come from any recent build, and nothing on the node records which. Two deploys minutes apart can load different code from the same tag.

---

# What follows from a movable tag

The consequence is not a single failure mode. It is the loss of a guarantee everything else assumed.

| Assumption | Why it no longer holds |
|---|---|
| "The tag holds the image from my commit" | Another run may have written it afterwards |
| "Two deploys of one blueprint load the same code" | The tag may have moved between them |
| "A green build means the deployed image was verified" | The build verified an image; the tag may point at another |
| "The registry copy matches the runner's local copy" | Only until the next upload lands |

- **A digest is the only reference that cannot move** — but no node in this project has ever been given one, so it is evidence rather than a deployment route.
- **Reading the tag at deploy time is what settles it**: query the registry for the digest behind `:latest` and record it beside the run.

---

<!-- _class: lead -->
![bg](../assets/bg-fpt-tower.jpg)

# Thank you

Questions welcome · sources linked throughout · GitHub Actions and Docker documentation are the authorities

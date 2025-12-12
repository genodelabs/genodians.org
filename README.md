**This repository has been migrated to
[codeberg.org](https://codeberg.org/genodelabs/genodians.org) and it will no
longer be updated at GitHub.**

Adapting to this change requires little adaptation on your side. Please adapt
any existing clone of this repository. Note that we also changed the name of
the default branch from "master" to "main". The instructions below
also demonstrate how to create a local "main" branch in your cloned repository.

```
genodians.org$ git set-url origin https://codeberg.org/genodelabs/genodians.org.git
genodians.org$ git fetch origin
genodians.org$ git checkout origin/main
genodians.org$ git switch -c main
```

Alternatively, you may also create a fresh clone:

```
$ git clone https://codeberg.org/genodelabs/genodians.org.git
```

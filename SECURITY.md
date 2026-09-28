# Security

Please report vulnerabilities privately through GitHub's
[private vulnerability reporting](../../security/advisories/new) rather than a public issue.

What the project does to keep the supply chain honest:

* Every downloaded binary or archive (OpenCV source, ONNX Runtime, YOLOX weights) is checked
  against a pinned SHA-256 before use (`scripts/_verify.sh`).
* The Docker base image is pinned by digest; the runtime image runs as a non-root user.
* Evaluation dependencies are pinned to exact commits / versions.
* CI runs with a read-only token and actions pinned to commit SHAs; Dependabot proposes updates.
* The renderer starts `ffmpeg` with an argument vector (no shell), so file paths are never
  interpreted as shell syntax.
* No datasets, model weights or credentials are stored in the repository.

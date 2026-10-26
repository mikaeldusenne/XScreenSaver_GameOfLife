# Enable automated builds

[`github-actions.yml`](github-actions.yml) is a complete GitHub Actions workflow
template. To activate it in your repository:

```sh
mkdir -p .github/workflows
cp ci/github-actions.yml .github/workflows/build.yml
git add .github/workflows/build.yml
git commit -m "Enable portable build checks"
git push
```

GitHub requires a credential allowed to write workflows. With the GitHub CLI,
an OAuth login may need `gh auth refresh -h github.com -s workflow` first.
The template is deliberately inactive until copied into `.github/workflows`.

It compiles and runs the existing simulation checks on Ubuntu x86-64 and ARM64,
macOS Apple Silicon and Intel, plus Alpine/musl. Linux also gets an Xvfb preview
smoke check and DEB packaging. Each regular runner uploads a native TGZ archive;
Linux also uploads a DEB. Both full and libXss-free builds are exercised.

macOS jobs build against Homebrew X11 libraries; they do not test the XQuartz
desktop. No BSD VM is configured. Build artifacts are specific to their runner
OS, architecture and library versions. The workflow has read-only repository
permissions and does not publish releases, install services or deploy anything.

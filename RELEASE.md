# Release Instructions

This file contains instructions how to publish a Qbs release into various package systems.

## Building packages

Release packages are built automatically by the
[Build release packages](https://github.com/qbs/qbs/actions/workflows/release.yml) job on GitHub
actions. The job builds every commit, so you'll need to find a specific run triggered by
pushing a git tag instead of a commit.

The only interesting artifact is qbs-release-v1.x.y - it contains Windows binary packages as
well as source packages.

Packages should then be uploaded to the
[Qt Website](https://download.qt.io/official_releases/qbs/).

## Chocolatey

For updating Qbs in [Chocolatey](https://community.chocolatey.org/packages/qbs), you'll need
to be a maintainer of the Qbs project. You'll also will need an API key from your
[account](https://community.chocolatey.org/account) (make sure you have one).

Get the qbs.1.x.y.nupkg file from the qbs-release-v1.x.y.zip archive and run the following command:

```
choco push --api-key <YOUR_API_KEY> qbs.1.x.y.nupkg"
```

Choco will upload the file, download binary packages from the Qt Website and publish Qbs
automatically.

## Homebrew

First, you'll need to [install](https://docs.brew.sh/Installation) Homewbrew.

Second, you'll need to fork the [homebrew-core](https://github.com/Homebrew/homebrew-core) repo
to you GitHub account.

Next you'll need to add your remote to the existing repo:
```
$ brew update
$ cd "$(brew --repository homebrew/core)"
$ git remote add github git@github.com:<USERNAME>/homebrew-core.git
# Create a new git branch for your formula so your pull request is easy to
# modify if any changes come up during review.
$ git checkout -b <some-descriptive-name> origin/master
```

You'll need a SHA-256 sum for the qbs-src-1.x.y.tar.gz source package. SHA-256 sum can be
found in the sha256sums.txt file from the qbs-release-v1.x.y.zip.

Check if the same upgrade has been already submitted by searching the open pull requests for
[Qbs](https://github.com/Homebrew/homebrew-core/pulls?q=is%3Apr+is%3Aopen+qbs).

Run the following command, replacing the version and SHA-256 sum:
```
$ brew bump-formula-pr --strict qbs \
    --url https://download.qt.io/official_releases/qbs/1.x.y/qbs-src-1.x.y.tar.gz \
    --sha256=<SHA-256>
```

The above can also be done manually by editing the formula source code and creating a pull request.

Some useful commands to build/test formula locally:
```
$ brew install --build-from-source qbs
$ brew test qbs
$ brew audit --strict qbs
```

## Mac Ports

Fork [macports-ports](https://github.com/macports/macports-ports) to your GitHub account
and clone the repo.

Run the following command in the repo dir (must be done only once):
```
$ sudo /opt/local/bin/portindex
```

Make sure that /opt/local/etc/macports/sources.conf contains a line like
file:///Users/abbapoh/dev/macports-ports [default]
pointing to your local repo.

Get the qbs-src-1.x.y.tar.gz source package from the archive or from the
[Qt Website](https://download.qt.io/official_releases/qbs/) and make note of the file size:
```
$ ls -l qbs-src-1.x.y.tar.gz
```
Get checksums:
```
$ openssl sha256 qbs-src-1.x.y.tar.gz
$ openssl rmd160 qbs-src-1.x.y.tar.gz
```

Update the devel/qbs/Portfile file with the new url, filesize and checksums.

Build the package:
```
$ /opt/local/bin/port lint qbs
$ sudo /opt/local/bin/port -vst install qbs
$ /opt/local/bin/qbs --version # Check that this reports the right version
```
Commit. Push. Create merge request.

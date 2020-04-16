# Contributing to Qbs

The main source code repository is hosted at
[codereview.qt-project.org](https://codereview.qt-project.org/q/project:qbs/qbs).

The Qbs source code is also mirrored on [code.qt.io](https://code.qt.io/cgit/qbs/qbs.git/)
and on [GitHub](https://github.com/qbs/qbs). Please note that those mirrors
are read-only and we do not accept pull-requests on GitHub. However, we gladly
accept contributions via [Gerrit](https://codereview.qt-project.org/q/project:qbs/qbs).

This document briefly describes steps required to be able to propose changes
to the Qbs project.

See the [Qt Contribution Guidelines](https://wiki.qt.io/Qt_Contribution_Guidelines)
page, [Setting up Gerrit](https://wiki.qt.io/Setting_up_Gerrit) and
[Gerrit Introduction](https://wiki.qt.io/Gerrit_Introduction) for more
details about how to upload patches to Gerrit.

## Preparations

* [Set up](https://wiki.qt.io/Setting_up_Gerrit#How_to_get_started_-_Gerrit_registration)
a Gerrit account
* Tweak your SSH config as instructed [here](https://wiki.qt.io/Setting_up_Gerrit#Local_Setup)
* Use the recommended Git settings, defined [here](https://wiki.qt.io/Setting_up_Gerrit#Configuring_Git)
* Get the source code as described below

## Cloning Qbs

Clone Qbs from the [code.qt.io](https://code.qt.io/cgit/qbs/qbs.git/) mirror
```
git clone git://code.qt.io/qbs/qbs.git
```
Alternatively, you can clone from the mirror on the [GitHub](https://github.com/qbs/qbs)
```
git clone https://github.com/qbs/qbs.git
```

Set up the Gerrit remote
```
git remote add gerrit ssh://<gerrit-username>@codereview.qt-project.org:29418/qbs/qbs
```

## Setting up git hooks

Install the hook generating Commit-Id files into your top level project directory:
```
gitdir=$(git rev-parse --git-dir); scp -P 29418 codereview.qt-project.org:hooks/commit-msg ${gitdir}/hooks/
```

This hook automatically adds a "Change-Id: …" line to the commit message. Change-Id is used
to identify new Patch Sets for existing
[Changes](https://wiki.qt.io/Gerrit_Introduction#Terminology).

## Making changes

Commit your changes in the usual way you do in Git.

After making changes, you might want to ensure that Qbs can be built and autotests pass:
```
qbs build -p autotest-runner
```
See ["Appendix A: Building Qbs"](http://doc.qt.io/qbs/building-qbs.html) for details.

In case your changes might significantly affect performance (in either way), the
'qbs_benchmarker' tool can be used (Linux only, requires Valgrind to be installed):
```
qbs_benchmarker -r <QBS_REPO> -o <OLD_REVISION> -n <NEW_REVISION> -a <ACTIVITY> -p <PROJECT>
```
Use 'qbs_benchmarker --help' for details.

## Pushing your changes to Gerrit

After committing your changes locally, push them to Gerrit

```
git push gerrit HEAD:refs/for/master
```

Gerrit will print a URL that can be used to access the newly created Change.

## Adding reviewers

Use the "ADD REVIEWER" button on the Change's web page to ask people to do the
review. To find possible reviewers, you can examine the Git history with
'git log' and/or 'git blame' or ask on the mailing list: qbs@qt-project.org

## Modifying Commits

During the review process, it might be necessary to do some changes to the commit.
To include you changes in the last commit, use
```
git commit --amend
```
This will edit the last commit instead of creating a new one. Make sure to preserve the
Change-Id footer when amending commits. For details, see
[Updating a Contribution With New Code](https://wiki.qt.io/Gerrit_Introduction#Updating_a_Contribution_With_New_Code)

## Abandoning changes

Changes which are inherently flawed or became inapplicable should be abandoned. You can do that
on the Change's web page with the "ABANDON" button. Make sure to remove the abandoned commit
from your working copy by using '[git reset](https://git-scm.com/docs/git-reset)'.

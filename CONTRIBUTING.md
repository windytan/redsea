# Contributing guidelines

## How to improve the wiki

It would be nice to have accessible, up-to-date documentation in the
[wiki](https://github.com/windytan/redsea/wiki). It can be edited by
anyone, and you're invited to improve it.

## How to create an issue

[Bug reports](https://github.com/windytan/redsea/issues), also known as
issues, are very useful for the development of redsea.

Some guidelines for making good bug reports:

* Include the **input signal** that reproduces the bug. If it's cumbersome to upload then don't
  worry, we can find ways around it.
* Include basic information about your system: Operating system? What kind
  of computer? Which version of redsea?
* Include any **error message** and any **warnings**.

## How to contribute source code

You can contribute to the source code via [pull requests](https://github.blog/developer-skills/github/beginners-guide-to-github-creating-a-pull-request/) (PRs). We have a build and test
pipeline in place to ensure that the build works and no basic functionality
breaks. So no worries.

Our maintainer will review the PR and may also be able to help complete the PR as time allows.
However, note that this is a hobby project and not so much time can be allotted.

Some rules we wish to follow:

* New features and changes in the main branch should come with some kind of a test.
  It can be a unit, component, or end-to-end test (see below) and it should reflect
  what is expected of the feature.
  * Ideally, all test data should come from some actual radio station and the source is cited. MPX files can be stored in git-lfs but their size should be manageable.
* A clear and concise [commit messages](https://www.gitkraken.com/learn/git/best-practices/git-commit-message)
  (PR description) helps a maintainer understand the reason behind the commit and the design choices.
* Use the included .clang-format file for formatting code.
* Whenever possible, try to follow the already existing conventions which are loosely following the [Google guidelines](https://google.github.io/styleguide/cppguide.html) for C++17.
* Try to avoid resorting to manual memory management. We have an address sanitizer in
  CI but currently no leak checks.
* If possible, we would really like to keep redsea fast enough to run in real time on a Raspberry Pi 1 Model B (see [benchmarks](https://github.com/windytan/redsea/wiki/Benchmark-results) for inspiration).

## Using AI

Avoid pushing raw LLM output from sites like ChatGPT, Claude, or Copilot. AI-assisted programming is
generally okay, but you yourself must examine and understand each line of code you submit.

It's much preferable to rewrite all code by yourself before making a PR.

Don't post any AI-generated content in the issues or discussions.

## How to build and run the tests

We have three kinds of tests: unit tests, component tests, and end-to-end tests. The difference between unit and component tests is hazy in this project.

To run the unit and
component tests, Install Catch2, then run these in the redsea project root directory:

```bash
git lfs pull
meson setup build -Dbuild_tests=true
cd build
meson test
```

The end-to-end tests are in a Perl script that runs the compiled binary. Some of them
require sox and git-lfs to prepare the test input files.

```bash
# (...commands to build redsea...)
sudo apt install git-lfs perl sox
# If you didn't already pull the test data
git lfs pull
perl test/end_to_end.pl
```

Inside the script, there's usage help for skipping the sox tests. Even though it's Perl, we tried to make it clear and very understandable and extensible for non-Perlists as well.

### Generate coverage report

```bash
meson setup -Db_coverage=true build -Dbuild_tests=true
cd build
meson test
ninja coverage-html
```

## Join the discussion

We have a [discussion section](https://github.com/windytan/redsea/discussions)
on GitHub for questions and brainstorming.

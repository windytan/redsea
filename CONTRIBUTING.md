# Contributing guidelines

## Edit the wiki

It would be nice to have accessible, up-to-date documentation in the
[wiki](https://github.com/windytan/redsea/wiki). It can be edited by
anyone, and you're invited to improve it.

## Create an issue

[Bug reports](https://github.com/windytan/redsea/issues) are very useful
for the development of redsea.

Some guidelines for making good bug reports:

* When creating a new bug report ("issue"), be sure to include **basic
  information** about your system:
  * Operating system version
  * CPU architecture
  * `redsea --version`
  * Did you compile the libraries (liquid-dsp, sndfile) yourself or are they
   from package repositories?
* If there is an **error message**, please include the error message verbatim in
  the bug report. If it is very long, consider putting it in a file or
  [gist](https://gist.github.com/) instead.
* If the problem only appears with some **input signal** it would be very helpful to
  have a copy of this signal for testing. If it's cumbersome to upload then don't
  worry, we can find ways around it.
* Be sure to **check back** with GitHub afterwards to see if I've asked any
  clarifying questions. I may not have access to an environment similar to
  yours and can't necessarily reproduce the bug myself, and that's why I
  may have many questions at first.
* If you fixed the problem yourself, it would be helpful to hear your
  solution! It's possible that others will also run into similar problems.
* Please be patient with it; Redsea is a single-maintainer hobby project.

## How to build and run the tests

Install Catch2, then in the redsea root run:

    git lfs pull
    meson setup build -Dbuild_tests=true
    cd build
    meson compile
    meson test

### Generate coverage report

    meson setup -Db_coverage=true build -Dbuild_tests=true
    cd build
    meson compile
    meson test
    ninja coverage-html

## General PR guidelines

You can directly contribute to the source code via pull requests. We have a
CI pipeline in place to ensure that the build works and no basic functionality
breaks. So no worries. Our maintainer can help complete the PR as time allows.
However, note that this is still a hobby project.

Let's face it, C++ is memory-unsafe and not the easiest language at that. But
there are things we can do to make it safer and more usable for us:

* Squashed commits with clear and concise [commit messages](https://www.gitkraken.com/learn/git/best-practices/git-commit-message) are preferred. Use force-push to update the PR branch if necessary.
* Since 1.0, we wish to have tests in repo for all new features or bugfixes. The tests
  can be unit tests written in [Catch2](https://github.com/catchorg/Catch2/) (see `test/unit.cc`),
  or they can be end-to-end tests against the CLI executable itself (some Perl examples
  in `test/cli.pl`). Perl is only used for testing; all code should pass Perl::Critic
  level 3.
* Ideally, all test data comes from some actual radio station and the source is cited.
* C++ style is described in `.clang-format`; format-on-save is recommended.
* We aim for C++14 compatibility, so unfortunately more modern features can't be used.
* We have some static analysis rules described in `.clang-tidy`.
* Keep in mind redsea has a real-time requirement and it might be run on some
  low-end 32-bit embedded platform. Unfortunately we don't have automated tests for
  this.
* Try to avoid resorting to manual memory management. We have an address sanitizer in
  CI but currently no leak checks.

Some design philosophy:
* Each JSON line should somehow correspond to a single RDS group. But this may
  well change in the future, for any good reason.
* All JSON output should validate successfully against `schema.json`.
* Data spread over multiple groups should be withdrawn until fully received, unless
  the user specifies the `--show-partial` option.
* The hex output should be compatible with RDS Spy, the (unaffiliated) Windows GUI software. (Yes,
  it's difficult to automate this test)
* GUI features are outside of our scope
* Preferably redsea should not create any files in the file system; instead all
  output should be via stdout if possible

The project uses [semantic versioning](https://semver.org/).

## Join the discussion

We have a [discussion section](https://github.com/windytan/redsea/discussions)
on GitHub for questions and brainstorming.

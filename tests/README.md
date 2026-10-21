# Manual executor notifications

Run the retained notification contract tests with:

```sh
cmake -S . -B build -DTMC_USE_HWLOC=OFF -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

The six tests cover header-only, standalone static, and standalone shared
implementations, each with runtime-selected and compile-time priority counts.
They check single and empty/nonempty bulk posts, concrete and type-erased
executors, both thread hints from inside and outside the executor, priority
promotion, publication before notification, cross-executor continuation
resumption, concurrent producers, null hooks, and callback replacement across
executor lifetimes.

The teardown test holds a callback after guarded publication and consumes its
queued work before stopping the consumer. No producer may begin another post.
It checks that teardown stays pending during a bounded observation interval and
finishes after the callback is released. This tests the documented supported
in-flight race, not unrestricted concurrent posting and teardown. CTest applies
a timeout to every test so missing notifications cannot hang the test runner.

# C++ Socket server epool coroutines (c++20)    

-   Sources, headers and mains separated in distinct folders
-   Use of modern [CMake](https://cmake.org/) for much easier compiling
-   Setup for tests using [doctest](https://github.com/onqtam/doctest)
-   Code coverage reports (TODO:review this) including automatic upload to [Coveralls.io](https://coveralls.io/) and/or [Codecov.io](https://codecov.io)
-   Code documentation with [Doxygen](http://www.stack.nl/~dimitri/doxygen/)

## Requirements

doxygen lcov build-essential gdb cmake

sudo apt install doxygen lcov build-essential gdb cmake


## Structure
``` text
.
├── CMakeLists.txt
├── app
│   └── main.cc
├── include
│   ├── example.h
│   ├── exampleConfig.h.in (TODO: review)
│   └── ...
├── src
│   └── example.cpp
└── tests
    ├── dummy.cpp
    └── main.cpp
```

Sources go in [src/](src/), header files in [include/](include/), main programs in [app/](app), and
tests go in [tests/](tests/) (compiled to `unit_tests` by default).

If you want to add a new executable, say `app/hello.cpp`, you only need to add the following two lines to [CMakeLists.txt](CMakeLists.txt):

``` cmake
add_executable(main app/main.cpp)   # Name of exec. and location of file.
target_link_libraries(main PRIVATE ${LIBRARY_NAME})  # Link the executable to lib built from src/*.cpp (if it uses it).
```

You can find `main` executable in [app/main.cc](app/main.cpp) under the `Build` section in [CMakeLists.txt](CMakeLists.txt).
If the executable you made does not use the library in [src/](src), then only the first line is needed.



## Building, Testing and Running

Build by making a build directory (i.e. `build/`), run `cmake` in that dir, and then use `make` to build the desired target.

Example:

``` bash
> mkdir build && cd build
> cmake .. -DCMAKE_BUILD_TYPE=[Debug | Coverage | Release]
> make
> ./main         # Runs the main executable
> python3 ../tests/python-testclient/pythonTcpClient.py #runs a sample python client 
> make test      # Makes and runs the tests.
> make coverage  # Makes and runs the tests, then Generates a coverage report. (requires "cmake .. -DCMAKE_BUILD_TYPE=Coverage")
> make doc       # Generate html documentation.
> ./unit_tests -s #runs unittests with details

> 
```

## .gitignore

The [.gitignore](.gitignore) file is a copy of the [Github C++.gitignore file](https://github.com/github/gitignore/blob/master/C%2B%2B.gitignore),
with the addition of ignoring the build directory (`build/`).

## Services (TODO)

If the repository is activated with Travis-CI, then unit tests will be built and executed on each commit.
The same is true if the repository is activated with Appveyor.

If the repository is activated with Coveralls/Codecov, then deployment to Travis will also calculate code coverage and
upload this to Coveralls.io and/or Codecov.io


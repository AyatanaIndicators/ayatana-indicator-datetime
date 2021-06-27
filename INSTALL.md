# Build and installation instructions

## Compile-time build dependencies

 - cmake (>= 3.13)
 - cmake-extras
 - libayatana-common (>= 0.9.3)
 - glib-2.0 (>= 2.36)
 - libical (>=0.48)
 - libecal-2.0 (>=3.16)
 - libedataserver-1.2 (>=3.5)
 - gstreamer-1.0 (>=1.2)
 - libnotify (>=0.7.6)
 - properties-cpp (>=0.0.1)
 - libaccounts-glib (>=1.18)
 - gettext (>= 0.18)
 - dbus
 - gcovr (>= 2.4)
 - lcov (>= 1.9)
 - gtest (>= 1.6.0)
 - dbus-test-runner
 - libdbustest1
 - python3-dbusmock
 - glibc-locales
 - gvfs-daemons
 - systemd

## For end-users and packagers

```
cd ayatana-indicator-datetime-X.Y.Z
mkdir build
cd build
cmake ..
make
sudo make install
```

**The install prefix defaults to `/usr`, change it with `-DCMAKE_INSTALL_PREFIX=/some/path`**

## For testers - unit tests only

```
cd ayatana-indicator-datetime-X.Y.Z
mkdir build
cd build
cmake .. -DENABLE_TESTS=ON
make
make test
make cppcheck
```

## For testers - both unit tests and code coverage

```
cd ayatana-indicator-datetime-X.Y.Z
mkdir build-coverage
cd build-coverage
cmake .. -DENABLE_COVERAGE=ON
make
make coverage-html
```

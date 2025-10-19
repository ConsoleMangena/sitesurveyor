# SiteSurveyor Desktop

Cross-platform Qt 6 desktop app.

## Downloads
- Latest Windows zip: https://github.com/ConsoleMangena/sitesurveyor/releases/latest/download/SiteSurveyor-Windows.zip
- Latest Debian/Ubuntu .deb (amd64): https://github.com/ConsoleMangena/sitesurveyor/releases/latest/download/SiteSurveyor-Debian-amd64.deb
- Download page: https://consolemangena.github.io/sitesurveyor/

## Build (Linux)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake qt6-base-dev qt6-base-dev-tools
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bin/SiteSurveyor
```

## Packaging (.deb)
```bash
sudo apt-get install -y debhelper
dpkg-buildpackage -us -uc -b
# Output in ../*.deb
```
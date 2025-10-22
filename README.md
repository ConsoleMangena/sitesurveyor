# SiteSurveyor Desktop

Cross-platform Qt 6 desktop app.

## Downloads
- Latest Windows zip: https://github.com/ConsoleMangena/sitesurveyor/releases/latest/download/SiteSurveyor-Windows.zip
- Latest Debian/Ubuntu .deb (amd64): https://github.com/ConsoleMangena/sitesurveyor/releases/latest/download/SiteSurveyor-Debian-amd64.deb
- Download page: https://sitesurveyor.dev

## Website
- Live site: https://sitesurveyor.dev
- GitHub Pages source: branch `gh-pages` (root).
- Content lives on `gh-pages`: `index.html` and `assets/`. To update the website, edit these files on `gh-pages` and push.
- Custom domain is configured via `CNAME` with: `sitesurveyor.dev`.

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

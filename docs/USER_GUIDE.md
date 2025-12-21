# SiteSurveyor User Guide

Welcome to SiteSurveyor! This guide will help you get started with the application.

## Table of Contents

- [Getting Started](#getting-started)
- [User Interface](#user-interface)
- [Survey Categories](#survey-categories)
- [Working with Projects](#working-with-projects)
- [Drawing Tools](#drawing-tools)
- [COGO Calculations](#cogo-calculations)
- [Network Adjustment](#network-adjustment)
- [Cloud Features](#cloud-features)
- [Keyboard Shortcuts](#keyboard-shortcuts)

---

## Getting Started

### First Launch

When you first launch SiteSurveyor, you'll see the Start Dialog with options to:

1. **Create a New Project** - Start fresh with a blank canvas
2. **Open a Project** - Load an existing `.ssp` project file
3. **Recent Projects** - Quick access to previously opened projects
4. **Templates** - Start from a category-specific template

### Choosing a Survey Category

Select the appropriate survey category for your project:

| Category | Best For |
|----------|----------|
| **Engineering** | Construction setout, as-built surveys, levelling |
| **Cadastral** | Boundary surveys, subdivisions, land registration |
| **Mining** | Volume calculations, development tracking, sections |
| **Topographic** | Contour mapping, feature surveys, DTM |
| **Geodetic** | Control networks, GNSS processing, baselines |

The category determines which tools are available and the default layers created.

---

## User Interface

### Main Window Layout

```
┌─────────────────────────────────────────────────────────────┐
│  Menu Bar                                                    │
├─────────────────────────────────────────────────────────────┤
│  Toolbar                                                     │
├──────────┬────────────────────────────────┬─────────────────┤
│          │                                │                 │
│  Layers  │        Canvas                  │  Properties     │
│  Panel   │                                │  Panel          │
│          │                                │                 │
│          │                                ├─────────────────┤
│          │                                │                 │
│          │                                │  Peg List       │
│          │                                │                 │
├──────────┴────────────────────────────────┴─────────────────┤
│  Status Bar (Coordinates | Zoom | CRS | Snap)               │
└─────────────────────────────────────────────────────────────┘
```

### Panels

- **Layers Panel** - Manage survey layers (visibility, color, lock)
- **Properties Panel** - View/edit selected entity properties
- **Peg List Panel** - Table of all pegs with coordinates
- **Console Panel** - Command console for scripting (optional)

### Themes

Switch between dark and light themes in **Edit → Settings → Appearance**.

---

## Survey Categories

### Engineering Surveying

**Default Layers:**
- Site Boundary
- Buildings
- Roads
- Services
- Setout Points
- Levels

**Available Tools:**
- Station Setup & Backsight
- Polar & Join Calculations
- Intersection & Resection
- Levelling
- Offset & Partition
- Network Adjustment

### Cadastral Surveying

**Default Layers:**
- Boundary
- Beacons
- Pegs
- Offset
- Servitudes
- Annotation

**Available Tools:**
- All core COGO tools
- Area Calculations
- Traverse Calculations
- Offset & Partition

### Mining Surveying

**Default Layers:**
- Ore Body
- Waste
- Development
- Ventilation
- Services
- Safety

**Available Tools:**
- Volume Calculations
- Cross-Sections
- Station Setup

### Topographic Surveying

**Default Layers:**
- Contours
- Spot Levels
- Buildings
- Vegetation
- Water
- Roads

**Available Tools:**
- Contour Generation
- DTM/TIN Creation
- Spot Heights

### Geodetic Surveying

**Default Layers:**
- Control Points
- Baselines
- Benchmarks
- Network

**Available Tools:**
- Traverse Calculations
- Network Adjustment
- Control Network Management

---

## Working with Projects

### Creating a New Project

1. Click **File → New** or use `Ctrl+N`
2. Select a survey category
3. Optionally choose a template
4. The canvas opens with default layers

### Saving Projects

- **Save**: `Ctrl+S` - Save to current location
- **Save As**: `Ctrl+Shift+S` - Save with a new name
- **Save to Cloud**: Sync to your cloud account

#### Project File Format

SiteSurveyor uses `.ssp` (SiteSurveyor Project) files, which are JSON-based and human-readable.

### Importing Data

| Format | Menu Location | Description |
|--------|---------------|-------------|
| DXF | File → Import → DXF | AutoCAD drawing exchange |
| Shapefile | File → Import → GDAL | Esri Shapefile (.shp) |
| GeoJSON | File → Import → GDAL | Geographic JSON |
| CSV Points | File → Import → CSV Points | Point data with coordinates |
| GeoTIFF | File → Import → GDAL | Georeferenced raster images |

### Exporting Data

| Format | Menu Location |
|--------|---------------|
| DXF | File → Export → DXF |
| Shapefile | File → Export → Shapefile |
| GeoJSON | File → Export → GeoJSON |
| CSV | File → Export → CSV |
| PDF | File → Print |

---

## Drawing Tools

### Point Tools

| Tool | Shortcut | Description |
|------|----------|-------------|
| Add Peg | `P` | Place a survey peg with name and coordinates |
| Add Point | - | Add a point entity |

### Line Tools

| Tool | Shortcut | Description |
|------|----------|-------------|
| Line | `L` | Draw a two-point line |
| Polyline | `PL` | Draw a multi-point polyline |
| Rectangle | `R` | Draw a rectangle |

### Arc & Circle Tools

| Tool | Shortcut | Description |
|------|----------|-------------|
| Circle | `C` | Draw a circle (center + radius) |
| Arc | `A` | Draw a three-point arc |

### Text

| Tool | Shortcut | Description |
|------|----------|-------------|
| Text | `T` | Place text annotation |

### Editing Tools

| Tool | Shortcut | Description |
|------|----------|-------------|
| Select | `Esc` | Select entities |
| Move | `M` | Move selected entities |
| Copy | `Ctrl+C` | Copy selection |
| Delete | `Del` | Delete selected |
| Explode | - | Break complex entities into primitives |
| Join | - | Join polylines |

---

## COGO Calculations

### Station Setup

Set up your instrument position before taking measurements:

1. Go to **Survey → Station Setup**
2. Enter station coordinates (or select from canvas)
3. Set instrument height
4. Enter backsight coordinates
5. Set backsight bearing (optional)

### Polar Calculation

Calculate coordinates from bearing and distance:

1. Go to **Survey → Polar**
2. Enter starting point (or select from canvas)
3. Enter bearing (DD°MM'SS" or decimal degrees)
4. Enter horizontal distance
5. Click **Calculate**
6. Optionally add result to canvas

### Join Calculation

Calculate bearing and distance between two points:

1. Go to **Survey → Join**
2. Enter or select two points
3. Click **Calculate**
4. Results show bearing and distance

### Intersection

**Bearing-Bearing Intersection:**
1. Enter two known points with bearings
2. Calculate intersection point

**Bearing-Distance Intersection:**
1. Enter point with bearing
2. Enter point with distance
3. Calculate two possible solutions

### Resection

Calculate your position from observations to known points:

1. Go to **Survey → Resection**
2. Enter at least 3 known points with observed bearings/distances
3. Click **Calculate**
4. Review residuals and accept solution

### Traverse

Process a survey traverse:

1. Go to **Survey → Traverse**
2. Import stations from pegs or enter manually
3. Enter observed bearings and distances
4. Calculate forward bearings and coordinates
5. Review closure error
6. Apply adjustment (Bowditch or Transit)
7. Export adjusted coordinates to canvas

---

## Network Adjustment

SiteSurveyor integrates with GNU Gama for rigorous least-squares adjustment.

### Running an Adjustment

1. Go to **Survey → Network Adjustment**
2. Select points and observations to include
3. Set adjustment parameters:
   - Coordinate system (local/global)
   - A priori standard errors
   - Confidence level
4. Click **Run Adjustment**
5. Review results:
   - Adjusted coordinates
   - Residuals
   - Chi-square test
   - Degrees of freedom

### Interpreting Results

| Metric | Good Value | Meaning |
|--------|------------|---------|
| Chi-square test | Passed | Observations consistent with model |
| Sigma0 | ~1.0 | A priori errors match reality |
| Max residual | < 3σ | No outliers |

---

## Cloud Features

### Signing In

1. Go to **File → Sign In**
2. Enter your email and password
3. Your profile appears on the start screen

### Saving to Cloud

1. Work on your project locally
2. Go to **File → Save to Cloud**
3. Enter a project name
4. Project syncs to your cloud account

### Version History

1. Open a cloud project
2. Go to **File → Version History**
3. View all saved versions
4. Restore any previous version

### Sharing Projects

1. Open a cloud project
2. Go to **File → Share**
3. Enter collaborator email
4. Choose permission level (view/edit)

---

## Keyboard Shortcuts

### File Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New Project |
| `Ctrl+O` | Open Project |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+P` | Print |

### Edit Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+C` | Copy |
| `Ctrl+V` | Paste |
| `Del` | Delete |
| `Ctrl+A` | Select All |

### View Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+0` | Zoom to Extents |
| `Ctrl++` | Zoom In |
| `Ctrl+-` | Zoom Out |
| `G` | Toggle Grid |

### Drawing Tools

| Shortcut | Action |
|----------|--------|
| `Esc` | Select Mode |
| `P` | Add Peg |
| `L` | Line |
| `PL` | Polyline |
| `C` | Circle |
| `A` | Arc |
| `R` | Rectangle |
| `T` | Text |
| `M` | Move |

### Snapping

| Shortcut | Action |
|----------|--------|
| `S` | Toggle Snapping |
| `Shift` (hold) | Temporarily disable snap |

---

## Tips & Tricks

1. **Double-click** on a peg to edit its properties
2. **Right-click** on layers for context menu options
3. Use **mouse wheel** to zoom in/out
4. Hold **middle mouse button** to pan
5. Press **Enter** to confirm polyline drawing
6. Use **Tab** to cycle through snap points

---

## Getting Help

- **Official Website**: [sitesurveyor.dev](https://sitesurveyor.dev)
- **GitHub Issues**: [Report bugs](https://github.com/ConsoleMangena/sitesurveyor/issues)
- **Keyboard Shortcuts**: Help → Keyboard Shortcuts

---

*SiteSurveyor - Professional Geomatics Software*
*© 2025 Eineva Incorporated*

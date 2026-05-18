# 🚀 Future Feature: Cloud-Backed Animation Lazy Loading

## The Problem: Storage & RAM Constraints
Currently, the ESP32-C3 is limited by its 4MB Flash and ~320KB RAM. While we have 43 high-quality animations ready in `.bin` packed format, the system can only safely store and index about 21-22 animations before hitting stability issues or running out of filesystem space.

## The Vision: "Animation Windowing"
To support an unlimited or much larger library of animations (43+), we propose a **Lazy Loading & Cloud Sync** system. This system will manage animations in "windows" rather than keeping the entire library on the device.

### Core Mechanism
1.  **Circular Buffer of Assets**: Maintain a local storage capacity for ~21 animations.
2.  **Played Tracking**: Track which animations have been played in the current cycle.
3.  **Lazy Replacement**: 
    *   Once a set of animations (e.g., 7 bins) has been played, they are marked for removal.
    *   The system connects to a cloud endpoint (e.g., a simple Firebase or AWS S3 bucket) via the lightweight WiFi mode.
    *   It downloads the next set of 7 animations that have *not* been played yet.
    *   These are written to LittleFS, replacing the old "played" files.
4.  **Zero-Downtime Playback**: The downloading happens in the background or during a dedicated "Sync Window" (e.g., at 3 AM) to ensure the clock remains responsive.

## Technical Requirements
- **Cloud API**: A simple REST endpoint or bucket to host the 43+ `.bin` files.
- **Manifest File**: A `manifest.json` on the cloud listing all available animation IDs and their checksums.
- **Enhanced Filesystem Manager**: Logic to handle "rolling" file deletions and writes on LittleFS without causing fragmentation.
- **WiFi Session Management**: Extending the current "Option B" (connect -> sync -> disconnect) to handle small file downloads.

## Example Workflow (7-Bin Window)
- **State A**: Device has Bins 1-21.
- **Activity**: Device plays Bins 1-7.
- **Trigger**: System identifies 7 empty slots.
- **Action**: Connects to cloud, deletes Bins 1-7, downloads Bins 22-28.
- **State B**: Device has Bins 8-28.

---

*This document serves as a roadmap for scaling the visual variety of the clock once the core hardware stability is finalized.*

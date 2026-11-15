<div align="center">

#  LARP OS
**A Minimalist, Offline Reverse-Engineering Operating System**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Language: C++](https://img.shields.io/badge/Language-C%2B%2B17-orange.svg)]()
[![OS: Buildroot](https://img.shields.io/badge/OS-Custom%20Buildroot-green.svg)]()

<img src="/assets/blackhole.gif" alt="LARP OS Demo" width="600"/>

</div>

---

## What is LARP OS?
LARP OS (Live Action Role Play OS) is not a normal application. It is a **custom, bootable Linux distribution** built entirely from source using Buildroot. 

It boots in seconds directly into a custom C++ reverse-engineering engine. It strips away the bloat of modern operating systems (no web browsers, no window managers, no package managers) to do exactly one thing: **safely analyze Linux ELF binaries without ever executing them.**

Because it operates completely offline and statically reads raw bytes, it is perfectly safe to analyze malwares and viruses without worrying about them escaping. LARP OS will scan it, map it, and visualize the loops without the host machine ever being at risk.

---

## The Data Structures & Algorithms (DSA) Behind It
LARP OS was built for a Data Structures & Algorithms course. Every single phase of the pipeline is powered by a custom-implemented DSA structure.

| Module | File | Data Structure / Algorithm | Why it was used |
| :--- | :--- | :--- | :--- |
| **Architect** | `graph.cpp` | **Graph (Adjacency List)** | To model the Control Flow Graph (CFG) where nodes are Basic Blocks and edges are jumps. |
| **Architect** | `graph.cpp` | **Hashmap (Open Addressing)** | To store the CFG for \(O(1)\) instant lookup of blocks by their memory address. |
| **Math** | `Dfs.cpp` | **DFS (Depth-First Search)** | To traverse the graph and generate Reverse Postorder numbering. |
| **Math** | `Dominator.cpp` | **Dominator Tree** | Uses Cooper's Algorithm to find the "boss" nodes, proving where loops exist (Back-Edges). |
| **Math** | `Heap.cpp` | **Binary Heap (Min-Heap)** | A Priority Queue to rank loops by complexity and push the most dangerous ones to the top. |
| **Visual**| `Renderer.cpp` | **BFS (Breadth-First Search)** | To calculate the X/Y grid layout so blocks are drawn side-by-side without overlapping. |
| **Visual**| `LRU.h` | **LRU Cache (DLL + Hashmap)** | To cache screen coordinates so the renderer doesn't recalculate where a block goes. |
| **Scanner** | `Scanner.h` | **SHA-256 & Trees** | Cryptographic hashing for known-bad files, and DFS for walking the USB directory tree. |

---

##  How It Works (The Pipeline)

### 1. The Virus Scanner (Triage)
Before analyzing a file, the Scanner checks it without executing it. It looks for disguised files (a PDF that is secretly an ELF), dangerous strings (`bash -i`, `/etc/shadow`), high entropy (packed/encrypted malware), and SHA-256 hash matches against known viruses.

### 2. The X-Ray (CFG)
It uses `libelf` and the **Capstone Engine** to translate raw machine code into assembly. It chops the code into Basic Blocks and builds a map. It then uses a **Dominator Tree** to find loops. 

### 3. The Visualization (TUI Renderer)
Using a custom LRU Cache and BFS layout, it draws the graph directly to the terminal. 
*   **Red Double Frame:** The Loop Header (the door into the loop).
*   **Yellow Round Frame:** The blocks inside the loop.
*   **Circle Arrow (↻):** A back-edge. The exact jump instruction that goes backward.

---

## 📸 Gallery

<div align="center">
  <img src="YOUR_TITLE_IMAGE.png" alt="LARP OS Boot" width="600"/>
  <br>
  <i>LARP OS booting and running the X-Ray engine.</i>
</div>
<br>
<div align="center">
  <img src="YOUR_SCANNER_IMAGE.png" alt="Malware Scanner" width="600"/>
  <br>
  <i>The Scanner detecting a malicious file on a USB drive.</i>
</div>

---

##  How to Use LARP OS (For Friends & Testers)

**You do NOT need to download 25 GB of source code.** You just need the final ISO.

### Option A: Run it in a Virtual Machine (Safest)
1. Download the `larpos.iso` file from the **Releases** tab on this repository.
2. Open VirtualBox or VMware.
3. Create a new Linux (Other Linux 64-bit) VM with **at least 1 GB of RAM**.
4. Mount the `.iso` file and boot. 

### Option B: Boot from a USB Stick (The Hacker Way)
1. Download `larpos.iso`.
2. Download **Rufus** (Windows) or **balenaEtcher** (Mac/Linux).
3. Flash the `.iso` to a USB drive (USB #1).
4. Plug USB #1 into any laptop and boot from it.
5. Plug in a second USB (USB #2) containing the suspicious files you want to analyze.

---

##  Building from Source (For Developers)
If you want to compile the OS yourself:
1. Clone this repository.
2. Download [Buildroot](https://buildroot.org/).
3. Copy the `larpos_defconfig` from the `os-build` folder into Buildroot.
4. Run `make larpos_defconfig`.
5. Copy the `overlay` folder into Buildroot.
6. Copy the C++ source code into `overlay/opt/larpos/`.
7. Run `make -j$(nproc)` and wait 30 minutes.


## 📜 License & Credits

### Black Hole Animation (ASCII Art)
The dynamic black hole animation/GIF used in this README was created by **JIN2 (@JIN2-ASCII)**. 

**License Terms:**
> Black hole
> Copyright (c) 2026 JIN2 (@jin_22ee). All rights reserved.

*You can view the original repository here: [JIN2-ASCII](https://github.com/JIN2-ASCII/-JIN2-ASCII)*

### LARP OS (The Operating System)
The custom C++ engine, OS build scripts, and documentation for LARP OS are open-source for educational purposes. 

**Authors:**
*   Minahil (Architect)
*   Rubaisha (Visuals)
*   Hasan (Math)

---
*This project is for educational purposes only. Do not use LARP OS to execute malware on production machines.*

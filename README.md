# My C/++ Projects

Welcome to my GitHub repository showcasing three C projects that I've worked on. Below, you'll find descriptions of each project along with links to their respective directories.

## 📑 Contents

* **[P6 – IPv4 Router](#p6--ipv4-router)**
* **[P5 – Event-Based Server](#p5--event-based-server)**
* **[P4 – Multithreading Library](#p4--multithreading-library)**
* **[P3 – Custom Shell](#p3--custom-shell)**
* **[P2 – Terminal GIS App](#p2--terminal-gis-app)**
* **[P1 – Puzzle Solvers](#p1--puzzle-solvers)**<br>
  <sub>(CUDA solvers temporarily removed due to research restrictions; enjoy the first puzzle I ever implemented in plain C!)</sub>

## Projects

### Project 6: IPv4 Router

  **Description:** Routing Packets in a Simplified Software Router (C++)

Overview: Created a fully functioning simplified IPv4 router.

Key Features:

Dynamic Route Installation: Implemented Router::add_route() to insert new routing entries (prefix, mask length, optional next-hop, and output interface) into a custom routing table.

Longest-Prefix Matching: In Router::route(), iterated incoming packets across all interfaces, performed a binary search over stored prefixes to find the best (longest) matching route for each destination IP.

TTL Handling & Packet Forwarding: Decremented the IP header’s TTL field, dropped packets on TTL expiry or on missing routes, and forwarded valid packets via the correct interface—either directly to the destination network or to a specified next-hop router—by invoking the existing send_datagram() logic from PA1.

Impact & Validation:

Successfully passed all provided unit tests in the reference VM environment.

Demonstrated robust handling of route updates, overlapping prefixes, and edge cases (e.g. default (“0.0.0.0/0”) route).

- **Directory:** [Project 6](Projects/P6%20-%20IPv4%20Router)

### Project 5 — Event-Based Music Library Server

- **Description:**  
  A non-blocking, epoll-driven **audio-streaming server**.  
  * Handles **thousands of concurrent TCP clients** on a single thread using edge-triggered `epoll` (Linux) / `kqueue` (macOS).  
  * Clients browse a music library, request tracks, and receive **streamed `.wav` / `.mp3` data** in realtime.  
  * Zero-copy send (`sendfile` where available) plus fixed-size buffers keep latency low.  
  * Graceful connection time-outs and back-pressure handling—no client can starve the event loop.

  **Quick start**

  ```bash
  cd Projects/P6
  make                     # builds server and CLI test client
  ./musicd ./music_folder  # serve all audio files in ./music_folder
  
- **Directory:** [Project 5](Projects/P5%-%Event-Based%Server)

### Project 4 — Multithreading Library

- **Description:**  
  User-level threading package with a minimal `pthread`-like API.

  * **Scheduling modes**
    * *Pre-emptive* – time-sliced round-robin via `setitimer` + `SIGALRM`.
    * *Co-operative* – threads run until they call `thread_yield()`.

  * **Sync primitives**  
    Lightweight mutexes and a basic condition variable, usable in either mode.

  * **API surface**  
    `thread_create`, `thread_join`, `thread_yield`, `mutex_lock / unlock`.

  Build with  
  ```bash
  make MODE=preemptive     # or MODE=cooperative
  ./demo                   # shows context-switching and locking

- **Directory:** [Project 4](Projects/P4)

### Project 3: Custom Shell

- **Description:** For this project, I implemented a basic shell capable of performing many of the operations of the native shell. The shell also supports man pages and vi, at least on my own machine.

The shell can execute executables with appropriate permissions found in directories listed in the $PATH variable, as well as those with absolute or relative paths (e.g., ./hello). Command line arguments can also be supplied to these programs. The shell is interactive and can be mildly scriptable. When in interactive mode, typing into the terminal program provides input to the stdin of other running executables, displaying the output of stdout and stderr.

The shell supports file redirection, piping, and reading commands from a script file, providing flexibility beyond interactive usage. While it can't exhaustively support all programs installed on teach.cs due to maintenance and bug-fixing constraints, it can utilize basic system programs like cd (as a special case), ls, mkdir, rm, grep, touch, man, cat, wc, and any programs developed for the course.

This involved a lot of C-string parsing, file I/O, and standard usage of fork, exec, wait, pipe, and dup, to ensure that all commands run in parallel.

To run it, simply use the command ./cscshell -i ./cscshell_init
and then start using it like you would your native shell!

- **Directory:** [Project 3](Projects/P3)

### Project 2: Terminal GIS application

- **Description:**
Welcome to Simple Street Map (SSM), my terminal-based street map application! With SSM, you can navigate through simplified street maps in Toronto using a convenient command-line interface. I developed this project to enhance my skills in C programming, particularly focusing on structures, arrays, and dynamic memory allocation. I use the A star algorithm to find the quickest path between two points in the city.

OpenStreetMap (OSM) is an open-source mapping platform that empowers users to contribute, edit, and access geospatial data freely. It provides detailed and up-to-date maps, covering various geographical features like roads, buildings, rivers, and points of interest. However, given the complexity of OSM, I've distilled its functionality in SSM to focus solely on navigating a motor vehicle within a simplified map area.

This interactive tool that behaves like a terminal-based database client. It starts by loading a map file, and upon success, it enters a shell session where you can use various commands such as 'node', 'way', 'find', 'path', and 'quit'.

With 'node', I can retrieve basic information about a specific node.
Using 'way', I can obtain basic details about a particular way.
'find' allows me to search for ways or nodes based on keywords:
'find way' lists way IDs matching a keyword in their names.
'find node' provides a list of unique nodes associated with ways containing specified keywords.
'path' offers two options:
'path time' calculates the time to traverse a sequence of nodes.
'path create' generates a path between two specified nodes.
Lastly, 'quit' gracefully exits the program.

Example usage:

### Load map file
$ ./ssmap maps/uoft.txt
maps/uoft.txt successfully loaded. 1924 nodes, 410 ways.
>>

### 'node' command
>> node 1
Node 1: (43.6675534, -79.3997042)

### 'way' command
>> way 1
Way 1: Queen's Park Crescent West

### 'find way' command
>> find way Bloor
0 147 148 149 205 206 207 264 282 290 291 292 334 335 351 352 353 386

### 'find node' command
>> find node Bloor
0 1 2 175 290 465 511 652 867 895 980 981 982 983 984 985 986 1052 1168 1209 1210 1211 1212 1213 1214 1215 1317 1363 1479 1480 1513 1550 1551 1552 1553 1554 1555 1556 1703 1704 1705 1744 1745 1854

### 'path time' command
>> path time 199 200 201 202
0.1201 minutes

### 'path create' command
>> path create 199 186
199 200 201 202 666 667 668 186

### 'quit' command
>> quit

- **Directory:** [Project 2](Projects/P2)

### Project 1: Langford Pairing Command Line Tool

- **Description:** 
Welcome to the Langford Pairing Command Line Tool! This tool allows you to work with Langford pairings, arranging numbers in a sequence with specific distances between identical pairs.

A Langford pairing arranges numbers such that each identical pair has a distance equal to its value.

The tool has two modes:

Check Mode: Verify if a sequence is a valid Langford pairing.
Create Mode: Generate a Langford pairing.

Example Usage:

### Check Mode:
Check a sequence as a Langford pairing:

$ ./langford 2 10 1 2 1 9 12 14 11 7 15 16 10 13 3 9 6 7 3 12 11 8 14 6 4 5 15 13 16 4 8 5

It's a Langford pairing!

$ ./langford 8 4 1 2 6 11 9 6 5 9 5 11 7 3 8 10 4 3 1 7 10 2

Not a Langford pairing.

### Create Mode:
Generate a Langford pairing for n:

$ ./langford -c 16

Langford pairing for n=16: [2, 10, 1, 2, 1, 9, 12, 14, 11, 7, 15, 16, 10, 13, 3, 9, 6, 7, 3, 12, 11, 8, 14, 6, 4, 5, 15, 13, 16, 4, 8, 5]

$ ./langford -c 9

No pairing found for n=9.


- **Directory:** [Project 1](Projects/P1)



# Smart Campus Route & Resource Planning System

A medium-sized DAA project based on the graph-algorithm ideas in the supplied senior project report. This implementation is an original project version with an integrated C++ HTTP server and a browser UI. It adds **minimum spanning tree resource planning** alongside the campus shortest-route feature.

## Features

- 12 illustrative campus locations and 21 weighted, undirected paths.
- Shortest walking route using Dijkstra's algorithm.
- Minimum-distance network planning using Prim's or Kruskal's minimum spanning tree (MST) algorithm.
- Linear search by building name, category, and description.
- Building directory sorted by name A-Z, name Z-A, or category.
- Interactive SVG campus graph with route/network highlighting.
- Simple C++17 server; no external C++ libraries, database, framework, API key, or internet connection required.
- API endpoints can be tested directly in a browser.

> **Data note:** Names, path connections, map coordinates, and weights are demonstration data. Distances are approximate example weights, not a surveyed map of Chandigarh University or any real campus. Edit the data in `src/server.cpp` before representing them as actual campus measurements.

## Requirements

### Windows
- A C++17 compiler such as MinGW-w64 g++ (Code::Blocks' compiler also works if `g++` is on PATH).

### Linux
- g++ with C++17 support.

The browser interface uses local HTML, CSS, JavaScript, and SVG. It does not need Node.js or a separate frontend build.

## Run on Windows

1. Extract the ZIP into a folder.
2. Open the extracted project folder.
3. Double-click `scripts/build_windows.bat` or run it from Command Prompt.
4. After a successful build, double-click `run_windows.bat`.
5. Open `http://127.0.0.1:8080` in your browser.
6. Keep the terminal window open while using the website. Press `Ctrl+C` to stop the server.

If `g++` is not found, install MinGW-w64 or configure the compiler that comes with Code::Blocks so `g++` is available from Command Prompt.

## Run on Linux

From the extracted project folder:

```bash
./scripts/build_linux.sh
./smart-campus
```

Then open `http://127.0.0.1:8080`. Stop the server with `Ctrl+C`.

## If port 8080 is busy

The default port is 8080. If it is busy, start the compiled server with another port, for example `./smart-campus 8081` on Linux or `smart-campus.exe 8081` on Windows, then open `http://127.0.0.1:8081`. The server binds only to `127.0.0.1` and is intended for a local student demonstration, not public deployment.

## Project structure

```text
SmartCampusRouteResourcePlanner/
├── src/
│   └── server.cpp             # HTTP server, campus graph, Dijkstra, Prim, Kruskal, search/sort APIs
├── web/
│   ├── index.html             # User interface
│   ├── styles.css             # Responsive styling
│   └── app.js                 # UI interaction and SVG graph rendering
├── scripts/
│   ├── build_windows.bat
│   └── build_linux.sh
├── report/
│   ├── Smart_Campus_Route_Resource_Planning_Report.docx
│   └── Smart_Campus_Route_Resource_Planning_Report.pdf
├── docs/
│   ├── API_REFERENCE.md
│   └── TEST_CASES.md
├── README.md
└── run_windows.bat
```

## API smoke tests

Start the server first, then in a second terminal run:

```bash
python scripts/test_api.py
```

For a non-default port, pass the base URL:

```bash
python scripts/test_api.py --base-url http://127.0.0.1:8081
```

The script checks static file serving, Dijkstra, Prim/Kruskal agreement, search and sorting. It does not replace manual checks of browser interaction.

## DAA concepts demonstrated

| Topic | Where it appears |
|---|---|
| Graph representation / adjacency list | `src/server.cpp`, `makeGraph()` |
| Greedy shortest path | `apiRoute()` uses Dijkstra |
| Priority queue / heap | Dijkstra and Prim |
| Minimum spanning tree | `apiNetwork()` with Prim or Kruskal |
| Disjoint Set Union (Union-Find) | Kruskal implementation |
| Linear search | `apiSearch()` |
| Sorting | `apiBuildings()` |
| Complexity analysis | Report and interface complexity labels |

## API routes

| Method and path | Purpose |
|---|---|
| `GET /api/health` | Server availability |
| `GET /api/campus` | Building and path data |
| `GET /api/route?from=gate&to=library` | Dijkstra shortest route |
| `GET /api/network?algorithm=prim` | Prim minimum spanning tree |
| `GET /api/network?algorithm=kruskal` | Kruskal minimum spanning tree |
| `GET /api/search?q=library` | Linear search of campus locations |
| `GET /api/buildings?sort=name` | Sorted building directory (`name`, `za`, or `category`) |

## How the algorithm works

- **Dijkstra:** starts at the selected source, repeatedly processes the unvisited node with the smallest known distance, relaxes its outgoing edges, and follows parent pointers to reconstruct the shortest route. All project edge weights are non-negative.
- **Prim:** grows a minimum spanning tree from the first campus node using a priority queue of candidate edges.
- **Kruskal:** sorts all edges by weight and uses Disjoint Set Union to accept edges that connect different components, avoiding cycles.
- **Linear search:** checks each building's searchable text for the requested term.
- **Sorting:** sorts the directory using a comparison function.

## Important learning note

This project is designed to be explainable in a DAA viva. Please personalize the campus dataset and report, run the test cases on your own machine, and make sure you can explain Dijkstra, MST, and the difference between Prim and Kruskal.

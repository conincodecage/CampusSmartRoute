#include <algorithm>
#include <cctype>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
static const socket_t BAD_SOCKET = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static const socket_t BAD_SOCKET = -1;
#endif

namespace fs = std::filesystem;

struct Building {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    int x;
    int y;
};

struct Edge {
    int u;
    int v;
    int distance;
};

struct AdjEdge {
    int to;
    int distance;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
};

static const std::vector<Building> BUILDINGS = {
    {"gate", "Main Gate", "Entry", "Main campus entrance and visitor access point.", 80, 260},
    {"admin", "Admin Block", "Administration", "Administrative offices and student services.", 230, 165},
    {"library", "Central Library", "Academic", "Library and quiet study spaces.", 420, 85},
    {"lab", "Computer Lab", "Academic", "Computer laboratories and practical classes.", 400, 235},
    {"hostel", "Hostel", "Residential", "Student residential building.", 220, 360},
    {"canteen", "Canteen", "Food", "Campus food and refreshment area.", 405, 365},
    {"medical", "Medical Centre", "Health", "First-aid and campus health support.", 590, 285},
    {"auditorium", "Auditorium", "Events", "Seminars, talks and campus events.", 590, 130},
    {"sports", "Sports Complex", "Sports", "Sports facilities and outdoor activities.", 750, 360},
    {"parking", "Parking Area", "Transport", "Designated campus parking area.", 750, 70},
    {"exam", "Examination Block", "Academic", "Examination and assessment offices.", 625, 430},
    {"student-centre", "Student Centre", "Student Services", "Student support and common services.", 430, 465}
};

// Edge weights represent approximate walking-path distances in metres.
// They are illustrative project data, not surveyed campus measurements.
static const std::vector<Edge> EDGES = {
    {0, 1, 145}, {0, 4, 210}, {1, 2, 175}, {1, 3, 165}, {1, 4, 235},
    {2, 3, 130}, {2, 7, 170}, {2, 9, 250}, {3, 4, 155}, {3, 5, 140},
    {3, 11, 240}, {4, 5, 205}, {5, 6, 185}, {5, 11, 115}, {5, 10, 220},
    {6, 7, 175}, {6, 8, 195}, {7, 9, 155}, {8, 9, 310}, {8, 10, 165},
    {10, 11, 150}
};

static std::vector<std::vector<AdjEdge>> makeGraph() {
    std::vector<std::vector<AdjEdge>> graph(BUILDINGS.size());
    for (const auto &edge : EDGES) {
        graph[edge.u].push_back({edge.v, edge.distance});
        graph[edge.v].push_back({edge.u, edge.distance});
    }
    return graph;
}

static const std::vector<std::vector<AdjEdge>> GRAPH = makeGraph();

static std::string jsonEscape(const std::string &value) {
    std::ostringstream out;
    for (unsigned char c : value) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    const char *hex = "0123456789abcdef";
                    out << "\\u00" << hex[c >> 4] << hex[c & 0x0f];
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    return out.str();
}

static std::string buildingJson(int index) {
    const auto &b = BUILDINGS.at(static_cast<std::size_t>(index));
    std::ostringstream out;
    out << "{\"id\":\"" << jsonEscape(b.id) << "\",\"name\":\"" << jsonEscape(b.name)
        << "\",\"category\":\"" << jsonEscape(b.category) << "\",\"description\":\""
        << jsonEscape(b.description) << "\",\"x\":" << b.x << ",\"y\":" << b.y << "}";
    return out.str();
}

static std::string buildingShortJson(int index) {
    const auto &b = BUILDINGS.at(static_cast<std::size_t>(index));
    return "{\"id\":\"" + jsonEscape(b.id) + "\",\"name\":\"" + jsonEscape(b.name) + "\"}";
}

static std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

static std::string urlDecode(const std::string &value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+' ) {
            result.push_back(' ');
        } else if (value[i] == '%' && i + 2 < value.size()) {
            auto hexValue = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hexValue(value[i + 1]);
            int lo = hexValue(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                result.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                result.push_back(value[i]);
            }
        } else {
            result.push_back(value[i]);
        }
    }
    return result;
}

static std::map<std::string, std::string> parseQuery(const std::string &target) {
    std::map<std::string, std::string> params;
    const auto question = target.find('?');
    if (question == std::string::npos) return params;
    std::string query = target.substr(question + 1);
    std::istringstream stream(query);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        const auto equals = pair.find('=');
        const std::string key = urlDecode(pair.substr(0, equals));
        const std::string value = equals == std::string::npos ? "" : urlDecode(pair.substr(equals + 1));
        params[key] = value;
    }
    return params;
}

static int findBuilding(const std::string &id) {
    for (std::size_t i = 0; i < BUILDINGS.size(); ++i) {
        if (BUILDINGS[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

static HttpResponse jsonResponse(const std::string &body, int status = 200) {
    HttpResponse response;
    response.status = status;
    response.contentType = "application/json; charset=utf-8";
    response.body = body;
    return response;
}

static std::string errorJson(const std::string &message) {
    return "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}";
}

static HttpResponse apiCampus() {
    std::ostringstream out;
    out << "{\"ok\":true,\"buildingCount\":" << BUILDINGS.size() << ",\"pathCount\":" << EDGES.size() << ",\"buildings\":[";
    for (std::size_t i = 0; i < BUILDINGS.size(); ++i) {
        if (i) out << ',';
        out << buildingJson(static_cast<int>(i));
    }
    out << "],\"paths\":[";
    for (std::size_t i = 0; i < EDGES.size(); ++i) {
        if (i) out << ',';
        const auto &e = EDGES[i];
        out << "{\"from\":\"" << BUILDINGS[e.u].id << "\",\"to\":\"" << BUILDINGS[e.v].id
            << "\",\"distance\":" << e.distance << "}";
    }
    out << "]}";
    return jsonResponse(out.str());
}

static HttpResponse apiRoute(const std::map<std::string, std::string> &params) {
    const auto sourceIt = params.find("from");
    const auto targetIt = params.find("to");
    if (sourceIt == params.end() || targetIt == params.end()) {
        return jsonResponse(errorJson("Provide both from and to building IDs."), 400);
    }
    const int source = findBuilding(sourceIt->second);
    const int target = findBuilding(targetIt->second);
    if (source < 0 || target < 0) {
        return jsonResponse(errorJson("Unknown building. Choose a building from the campus list."), 400);
    }

    const int n = static_cast<int>(BUILDINGS.size());
    const int INF = INT_MAX / 4;
    std::vector<int> distance(n, INF), parent(n, -1);
    std::vector<bool> settled(n, false);
    using QueueItem = std::pair<int, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pq;
    distance[source] = 0;
    pq.push({0, source});
    int settledCount = 0;

    while (!pq.empty()) {
        const auto [currentDistance, u] = pq.top();
        pq.pop();
        if (currentDistance != distance[u] || settled[u]) continue;
        settled[u] = true;
        ++settledCount;
        if (u == target) break;
        for (const auto &edge : GRAPH[u]) {
            if (distance[u] + edge.distance < distance[edge.to]) {
                distance[edge.to] = distance[u] + edge.distance;
                parent[edge.to] = u;
                pq.push({distance[edge.to], edge.to});
            }
        }
    }

    if (distance[target] == INF) {
        return jsonResponse(errorJson("No route exists between those buildings."), 404);
    }

    std::vector<int> path;
    for (int at = target; at != -1; at = parent[at]) path.push_back(at);
    std::reverse(path.begin(), path.end());

    std::ostringstream out;
    out << "{\"ok\":true,\"algorithm\":\"Dijkstra\",\"from\":\"" << BUILDINGS[source].id
        << "\",\"to\":\"" << BUILDINGS[target].id << "\",\"distanceM\":" << distance[target]
        << ",\"settledNodes\":" << settledCount << ",\"path\":[";
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i) out << ',';
        out << buildingShortJson(path[i]);
    }
    out << "]}";
    return jsonResponse(out.str());
}

struct DisjointSet {
    std::vector<int> parent;
    std::vector<int> rank;
    explicit DisjointSet(int n) : parent(n), rank(n, 0) {
        for (int i = 0; i < n; ++i) parent[i] = i;
    }
    int find(int x) {
        if (parent[x] != x) parent[x] = find(parent[x]);
        return parent[x];
    }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        if (rank[a] < rank[b]) std::swap(a, b);
        parent[b] = a;
        if (rank[a] == rank[b]) ++rank[a];
        return true;
    }
};

struct ChosenEdge { int u; int v; int distance; };

static HttpResponse apiNetwork(const std::map<std::string, std::string> &params) {
    std::string algorithm = "prim";
    const auto it = params.find("algorithm");
    if (it != params.end()) algorithm = lowerCopy(it->second);
    if (algorithm != "prim" && algorithm != "kruskal") {
        return jsonResponse(errorJson("Algorithm must be prim or kruskal."), 400);
    }

    std::vector<ChosenEdge> chosen;
    int totalDistance = 0;
    if (algorithm == "prim") {
        std::vector<bool> visited(BUILDINGS.size(), false);
        using QueueItem = std::tuple<int, int, int>; // distance, from, to
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pq;
        visited[0] = true;
        for (const auto &e : GRAPH[0]) pq.push({e.distance, 0, e.to});
        while (!pq.empty() && chosen.size() < BUILDINGS.size() - 1) {
            const auto [weight, from, to] = pq.top(); pq.pop();
            if (visited[to]) continue;
            visited[to] = true;
            chosen.push_back({from, to, weight});
            totalDistance += weight;
            for (const auto &e : GRAPH[to]) {
                if (!visited[e.to]) pq.push({e.distance, to, e.to});
            }
        }
    } else {
        std::vector<Edge> sortedEdges = EDGES;
        std::sort(sortedEdges.begin(), sortedEdges.end(), [](const Edge &a, const Edge &b) {
            if (a.distance != b.distance) return a.distance < b.distance;
            if (a.u != b.u) return a.u < b.u;
            return a.v < b.v;
        });
        DisjointSet dsu(static_cast<int>(BUILDINGS.size()));
        for (const auto &e : sortedEdges) {
            if (dsu.unite(e.u, e.v)) {
                chosen.push_back({e.u, e.v, e.distance});
                totalDistance += e.distance;
                if (chosen.size() == BUILDINGS.size() - 1) break;
            }
        }
    }

    const bool connected = chosen.size() == BUILDINGS.size() - 1;
    std::ostringstream out;
    out << "{\"ok\":true,\"algorithm\":\"" << (algorithm == "prim" ? "Prim" : "Kruskal")
        << "\",\"connected\":" << (connected ? "true" : "false")
        << ",\"buildingCount\":" << BUILDINGS.size() << ",\"edgeCount\":" << chosen.size()
        << ",\"totalDistanceM\":" << totalDistance << ",\"edges\":[";
    for (std::size_t i = 0; i < chosen.size(); ++i) {
        if (i) out << ',';
        const auto &e = chosen[i];
        out << "{\"from\":" << buildingShortJson(e.u) << ",\"to\":" << buildingShortJson(e.v)
            << ",\"distanceM\":" << e.distance << "}";
    }
    out << "]}";
    return jsonResponse(out.str());
}

static HttpResponse apiSearch(const std::map<std::string, std::string> &params) {
    auto it = params.find("q");
    const std::string q = it == params.end() ? "" : lowerCopy(it->second);
    std::vector<int> results;
    // A simple linear search over the small campus dataset.
    for (std::size_t i = 0; i < BUILDINGS.size(); ++i) {
        const auto &b = BUILDINGS[i];
        const std::string searchable = lowerCopy(b.name + " " + b.category + " " + b.description);
        if (q.empty() || searchable.find(q) != std::string::npos) results.push_back(static_cast<int>(i));
    }
    std::ostringstream out;
    out << "{\"ok\":true,\"algorithm\":\"Linear Search\",\"complexity\":\"O(V)\",\"query\":\""
        << jsonEscape(q) << "\",\"count\":" << results.size() << ",\"results\":[";
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (i) out << ',';
        out << buildingJson(results[i]);
    }
    out << "]}";
    return jsonResponse(out.str());
}

static HttpResponse apiBuildings(const std::map<std::string, std::string> &params) {
    std::string sortBy = "name";
    auto it = params.find("sort");
    if (it != params.end()) sortBy = lowerCopy(it->second);
    if (sortBy != "name" && sortBy != "za" && sortBy != "category") sortBy = "name";
    std::vector<int> indices(BUILDINGS.size());
    for (std::size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<int>(i);
    std::stable_sort(indices.begin(), indices.end(), [sortBy](int a, int b) {
        const auto &left = BUILDINGS[a];
        const auto &right = BUILDINGS[b];
        if (sortBy == "category") {
            if (left.category != right.category) return left.category < right.category;
            return left.name < right.name;
        }
        if (sortBy == "za") return left.name > right.name;
        return left.name < right.name;
    });
    std::ostringstream out;
    out << "{\"ok\":true,\"sort\":\"" << sortBy << "\",\"complexity\":\"O(V log V)\",\"buildings\":[";
    for (std::size_t i = 0; i < indices.size(); ++i) {
        if (i) out << ',';
        out << buildingJson(indices[i]);
    }
    out << "]}";
    return jsonResponse(out.str());
}

static HttpResponse handleApi(const std::string &target) {
    const auto qmark = target.find('?');
    const std::string path = target.substr(0, qmark);
    const auto params = parseQuery(target);
    if (path == "/api/health") return jsonResponse("{\"ok\":true,\"service\":\"Smart Campus Route & Resource Planner\"}");
    if (path == "/api/campus") return apiCampus();
    if (path == "/api/route") return apiRoute(params);
    if (path == "/api/network") return apiNetwork(params);
    if (path == "/api/search") return apiSearch(params);
    if (path == "/api/buildings") return apiBuildings(params);
    return jsonResponse(errorJson("API endpoint not found."), 404);
}

static std::string mimeType(const fs::path &path) {
    const std::string ext = lowerCopy(path.extension().string());
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".png") return "image/png";
    if (ext == ".ico") return "image/x-icon";
    return "application/octet-stream";
}

static HttpResponse serveStatic(const std::string &urlPath) {
    std::string relative = urlPath;
    if (relative == "/" || relative.empty()) relative = "/index.html";
    if (relative.find("..") != std::string::npos || relative.find('\\') != std::string::npos) {
        return {403, "text/plain; charset=utf-8", "Forbidden"};
    }
    if (!relative.empty() && relative.front() == '/') relative.erase(relative.begin());
    const fs::path filePath = fs::path("web") / fs::path(relative);
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return {404, "text/plain; charset=utf-8", "Not found: " + relative};
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return {200, mimeType(filePath), buffer.str()};
}

static void closeSocket(socket_t client) {
#ifdef _WIN32
    closesocket(client);
#else
    close(client);
#endif
}

static bool sendAll(socket_t client, const std::string &data) {
    std::size_t sentTotal = 0;
    while (sentTotal < data.size()) {
#ifdef _WIN32
        const int sent = send(client, data.data() + sentTotal,
            static_cast<int>(std::min<std::size_t>(data.size() - sentTotal, INT_MAX)), 0);
#else
        const auto sent = send(client, data.data() + sentTotal, data.size() - sentTotal, MSG_NOSIGNAL);
#endif
        if (sent <= 0) return false;
        sentTotal += static_cast<std::size_t>(sent);
    }
    return true;
}

static std::string statusText(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        default: return "Internal Server Error";
    }
}

static void handleClient(socket_t client) {
    char buffer[16384];
#ifdef _WIN32
    const int received = recv(client, buffer, static_cast<int>(sizeof(buffer) - 1), 0);
#else
    const auto received = recv(client, buffer, sizeof(buffer) - 1, 0);
#endif
    if (received <= 0) { closeSocket(client); return; }
    buffer[received] = '\0';
    std::istringstream request(buffer);
    std::string method, target, version;
    request >> method >> target >> version;
    if (method.empty() || target.empty()) { closeSocket(client); return; }

    HttpResponse response;
    if (method != "GET" && method != "HEAD") {
        response = {405, "text/plain; charset=utf-8", "Only GET and HEAD are supported."};
    } else if (target.rfind("/api/", 0) == 0) {
        response = handleApi(target);
    } else {
        const auto qmark = target.find('?');
        response = serveStatic(target.substr(0, qmark));
    }

    std::ostringstream headers;
    headers << "HTTP/1.1 " << response.status << ' ' << statusText(response.status) << "\r\n"
            << "Content-Type: " << response.contentType << "\r\n"
            << "Content-Length: " << response.body.size() << "\r\n"
            << "Cache-Control: no-store\r\n"
            << "X-Content-Type-Options: nosniff\r\n"
            << "Connection: close\r\n\r\n";
    sendAll(client, headers.str());
    if (method != "HEAD") sendAll(client, response.body);
    closeSocket(client);
}

int main(int argc, char **argv) {
    int port = 8080;
    if (argc >= 2) {
        try { port = std::stoi(argv[1]); }
        catch (...) { std::cerr << "Usage: smart-campus [port]\n"; return 1; }
        if (port < 1024 || port > 65535) { std::cerr << "Port must be between 1024 and 65535.\n"; return 1; }
    }
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Could not initialize Windows sockets.\n";
        return 1;
    }
#endif

    socket_t server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == BAD_SOCKET) {
        std::cerr << "Could not create server socket.\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    int reuse = 1;
#ifdef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // local-machine access only
    if (bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        std::cerr << "Could not bind to 127.0.0.1:" << port << ". The port may already be in use.\n";
        closeSocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    if (listen(server, 16) < 0) {
        std::cerr << "Could not start listening on port 8080.\n";
        closeSocket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "\nSmart Campus Route & Resource Planning System\n"
              << "Open: http://127.0.0.1:" << port << "\n"
              << "Keep this terminal open while using the application.\n"
              << "Press Ctrl+C to stop the server.\n\n" << std::flush;

    while (true) {
        sockaddr_in clientAddress{};
#ifdef _WIN32
        int addressLength = sizeof(clientAddress);
#else
        socklen_t addressLength = sizeof(clientAddress);
#endif
        socket_t client = accept(server, reinterpret_cast<sockaddr *>(&clientAddress), &addressLength);
        if (client == BAD_SOCKET) continue;
        handleClient(client);
    }

    closeSocket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

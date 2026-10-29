'use strict';

const $ = (selector) => document.querySelector(selector);
const state = { campus: null, activeRoute: null, activeNetwork: null, toastTimer: null };
const SVG_NS = 'http://www.w3.org/2000/svg';

async function api(url) {
  const response = await fetch(url, { headers: { Accept: 'application/json' } });
  const data = await response.json();
  if (!response.ok || data.ok === false) throw new Error(data.error || `Request failed (${response.status})`);
  return data;
}

function toast(message, isError = false) {
  const element = $('#toast');
  element.textContent = message;
  element.classList.toggle('error', isError);
  element.classList.add('show');
  if (state.toastTimer) clearTimeout(state.toastTimer);
  state.toastTimer = setTimeout(() => element.classList.remove('show'), 3200);
}

function setServerStatus(status, message) {
  const dot = document.querySelector('.state-dot');
  dot.classList.remove('online', 'offline');
  if (status === 'online') dot.classList.add('online');
  if (status === 'offline') dot.classList.add('offline');
  $('#serverStatus').textContent = message;
}

function makeSvg(name, attrs = {}, text = '') {
  const element = document.createElementNS(SVG_NS, name);
  for (const [key, value] of Object.entries(attrs)) element.setAttribute(key, String(value));
  if (text) element.textContent = text;
  return element;
}

function edgeKey(a, b) { return [a, b].sort().join('|'); }

function renderCampus(data) {
  state.campus = data;
  $('#heroBuildingCount').textContent = data.buildingCount;
  $('#heroPathCount').textContent = `${data.pathCount} path links`;
  const source = $('#sourceSelect');
  const destination = $('#destinationSelect');
  source.replaceChildren();
  destination.replaceChildren();
  for (const building of data.buildings) {
    source.add(new Option(building.name, building.id));
    destination.add(new Option(building.name, building.id));
  }
  source.value = 'gate';
  destination.value = 'library';
  drawMap();
  updateNodeSelections();
}

function drawMap() {
  if (!state.campus) return;
  const svg = $('#campusMap');
  svg.replaceChildren();

  const background = makeSvg('rect', { x: 0, y: 0, width: 840, height: 520, rx: 10, fill: 'rgba(8,17,29,0.24)' });
  svg.append(background);

  const route = makeSvg('g', { 'aria-hidden': 'true' });
  const nodesLayer = makeSvg('g', {});
  const nodeById = new Map(state.campus.buildings.map((building) => [building.id, building]));

  for (const path of state.campus.paths) {
    const a = nodeById.get(path.from);
    const b = nodeById.get(path.to);
    if (!a || !b) continue;
    const line = makeSvg('line', {
      x1: a.x, y1: a.y, x2: b.x, y2: b.y,
      class: 'edge', 'data-edge': edgeKey(path.from, path.to),
      'stroke-linecap': 'round'
    });
    const title = makeSvg('title', {}, `${a.name} to ${b.name}: approximately ${path.distance} m`);
    line.append(title);
    route.append(line);
  }

  const initials = {
    gate: 'MG', admin: 'AD', library: 'LB', lab: 'CL', hostel: 'HO',
    canteen: 'CA', medical: 'MC', auditorium: 'AU', sports: 'SP',
    parking: 'PK', exam: 'EX', 'student-centre': 'SC'
  };

  for (const building of state.campus.buildings) {
    const group = makeSvg('g', {
      class: 'map-node', 'data-building': building.id, tabindex: 0, role: 'button',
      'aria-label': `Use ${building.name} as the selected map location`
    });
    group.append(makeSvg('circle', { cx: building.x, cy: building.y, r: 19, class: 'node-shadow' }));
    group.append(makeSvg('circle', { cx: building.x, cy: building.y, r: 13, class: 'node-core' }));
    group.append(makeSvg('text', { x: building.x, y: building.y + 3.5, 'text-anchor': 'middle', class: 'node-initial' }, initials[building.id] || building.name.slice(0, 2).toUpperCase()));
    group.append(makeSvg('text', { x: building.x, y: building.y + 32, 'text-anchor': 'middle', class: 'node-label' }, building.name));
    group.append(makeSvg('title', {}, `${building.name} - ${building.category}`));
    nodesLayer.append(group);
  }
  svg.append(route, nodesLayer);
}

function updateNodeSelections() {
  document.querySelectorAll('.map-node').forEach((node) => {
    node.classList.toggle('selected-from', node.dataset.building === $('#sourceSelect').value);
    node.classList.toggle('selected-to', node.dataset.building === $('#destinationSelect').value);
  });
}

function clearRouteHighlights() {
  document.querySelectorAll('.edge.route-edge').forEach((edge) => edge.classList.remove('route-edge'));
}
function clearNetworkHighlights() {
  document.querySelectorAll('.edge.mst-edge').forEach((edge) => edge.classList.remove('mst-edge'));
}
function applyRouteHighlights(routeData) {
  clearRouteHighlights();
  for (let i = 0; i < routeData.path.length - 1; i += 1) {
    const key = edgeKey(routeData.path[i].id, routeData.path[i + 1].id);
    const line = document.querySelector(`[data-edge="${CSS.escape(key)}"]`);
    if (line) line.classList.add('route-edge');
  }
}
function applyNetworkHighlights(networkData) {
  clearNetworkHighlights();
  for (const edge of networkData.edges) {
    const key = edgeKey(edge.from.id, edge.to.id);
    const line = document.querySelector(`[data-edge="${CSS.escape(key)}"]`);
    if (line) line.classList.add('mst-edge');
  }
}

function renderRoute(data) {
  state.activeRoute = data;
  const box = $('#routeResult');
  const steps = data.path.map((building, index) =>
    `${index ? '<span class="route-arrow" aria-hidden="true">→</span>' : ''}<span class="route-step">${escapeHtml(building.name)}</span>`
  ).join('');
  box.innerHTML = `
    <div class="route-summary">
      <div><small>SHORTEST WALKING DISTANCE</small><div class="route-distance">${data.distanceM}<span> metres</span></div></div>
      <span class="algorithm-pill">${data.algorithm}</span>
    </div>
    <div class="route-steps">${steps}</div>
    <div class="route-foot"><span>${data.path.length} location${data.path.length === 1 ? '' : 's'} in route</span><span>${data.settledNodes} nodes settled</span></div>`;
  applyRouteHighlights(data);
  updateNodeSelections();
}

function renderNetwork(data) {
  state.activeNetwork = data;
  const box = $('#networkResult');
  const cards = data.edges.map((edge, index) => `
    <div class="network-edge">
      <div class="edge-names"><small>LINK ${String(index + 1).padStart(2, '0')}</small><span>${escapeHtml(edge.from.name)}<br>↳ ${escapeHtml(edge.to.name)}</span></div>
      <strong>${edge.distanceM} m</strong>
    </div>`).join('');
  box.innerHTML = `
    <div class="network-summary">
      <div class="network-stat"><small>ALGORITHM</small><strong>${data.algorithm}</strong></div>
      <div class="network-stat"><small>SELECTED LINKS</small><strong>${data.edgeCount}<span> / ${Math.max(0, data.buildingCount - 1)}</span></strong></div>
      <div class="network-stat"><small>TOTAL LINK DISTANCE</small><strong class="orange-text">${data.totalDistanceM}<span> m</span></strong></div>
    </div>
    <div class="network-edges">${cards}</div>
    <p class="network-footnote">A spanning tree connects all ${data.buildingCount} locations using ${data.edgeCount} links without cycles. Distances are illustrative and should be surveyed before real deployment.</p>`;
  applyNetworkHighlights(data);
}

function renderSearch(data) {
  $('#searchMeta').textContent = `${data.count} result${data.count === 1 ? '' : 's'} · Linear Search · ${data.complexity}`;
  const box = $('#searchResults');
  if (!data.results.length) {
    box.innerHTML = '<p class="muted-message">No matching campus locations. Try a different name or category.</p>';
    return;
  }
  box.innerHTML = data.results.map((building) => `
    <div class="location-result">
      <div><strong>${escapeHtml(building.name)}</strong><small>${escapeHtml(building.description)}</small></div>
      <div class="result-actions"><span class="category-chip">${escapeHtml(building.category)}</span><button type="button" class="directory-go" data-use-destination="${escapeHtml(building.id)}" aria-label="Set ${escapeHtml(building.name)} as destination">→</button></div>
    </div>`).join('');
}

function renderDirectory(data) {
  $('#directoryCount').textContent = `${data.buildings.length} locations`;
  $('#directoryList').innerHTML = data.buildings.map((building, index) => `
    <div class="directory-item">
      <span class="directory-index">${String(index + 1).padStart(2, '0')}</span>
      <div class="directory-info"><strong>${escapeHtml(building.name)}</strong><small>${escapeHtml(building.category)}</small></div>
      <button type="button" class="directory-go" data-use-destination="${escapeHtml(building.id)}" aria-label="Set ${escapeHtml(building.name)} as destination">→</button>
    </div>`).join('');
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (char) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  })[char]);
}

async function findRoute(event) {
  if (event) event.preventDefault();
  const from = $('#sourceSelect').value;
  const to = $('#destinationSelect').value;
  const button = $('#routeForm button[type="submit"]');
  button.disabled = true;
  try {
    const data = await api(`/api/route?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`);
    renderRoute(data);
    toast('Shortest route calculated with Dijkstra.');
  } catch (error) {
    toast(error.message, true);
  } finally {
    button.disabled = false;
  }
}

async function planNetwork() {
  const button = $('#buildNetwork');
  button.disabled = true;
  try {
    const algorithm = $('#mstAlgorithm').value;
    const data = await api(`/api/network?algorithm=${encodeURIComponent(algorithm)}`);
    renderNetwork(data);
    toast(`${data.algorithm} planned a network of ${data.edgeCount} links.`);
  } catch (error) {
    toast(error.message, true);
  } finally {
    button.disabled = false;
  }
}

async function searchBuildings(event) {
  if (event) event.preventDefault();
  try {
    const q = $('#searchInput').value.trim();
    renderSearch(await api(`/api/search?q=${encodeURIComponent(q)}`));
  } catch (error) {
    toast(error.message, true);
  }
}

async function loadDirectory() {
  try {
    renderDirectory(await api(`/api/buildings?sort=${encodeURIComponent($('#directorySort').value)}`));
  } catch (error) {
    $('#directoryList').innerHTML = `<p class="muted-message">${escapeHtml(error.message)}</p>`;
  }
}

function resetMap() {
  clearRouteHighlights();
  clearNetworkHighlights();
  state.activeRoute = null;
  state.activeNetwork = null;
  $('#routeResult').innerHTML = '<div class="result-empty"><span class="empty-icon">⌖</span><strong>Your route will appear here</strong><p>Select a start and destination, then run Dijkstra.</p></div>';
  $('#networkResult').innerHTML = '<div class="result-empty compact-empty"><span class="empty-icon">⌘</span><strong>Your minimum network will appear here</strong><p>Choose Prim or Kruskal and run the planner.</p></div>';
  updateNodeSelections();
  toast('Map highlights cleared.');
}

function setSelectedBuilding(id) {
  const mode = $('#mapPickMode').value;
  const select = mode === 'from' ? $('#sourceSelect') : $('#destinationSelect');
  select.value = id;
  updateNodeSelections();
  const building = state.campus.buildings.find((entry) => entry.id === id);
  if (building) toast(`${building.name} set as ${mode === 'from' ? 'starting location' : 'destination'}.`);
}

function wireEvents() {
  $('#routeForm').addEventListener('submit', findRoute);
  $('#swapRoute').addEventListener('click', () => {
    const source = $('#sourceSelect').value;
    $('#sourceSelect').value = $('#destinationSelect').value;
    $('#destinationSelect').value = source;
    updateNodeSelections();
    findRoute();
  });
  $('#sourceSelect').addEventListener('change', updateNodeSelections);
  $('#destinationSelect').addEventListener('change', updateNodeSelections);
  $('#buildNetwork').addEventListener('click', planNetwork);
  $('#resetMap').addEventListener('click', resetMap);
  $('#searchForm').addEventListener('submit', searchBuildings);
  $('#directorySort').addEventListener('change', loadDirectory);

  $('#campusMap').addEventListener('click', (event) => {
    const node = event.target.closest('[data-building]');
    if (node) setSelectedBuilding(node.dataset.building);
  });
  $('#campusMap').addEventListener('keydown', (event) => {
    const node = event.target.closest('[data-building]');
    if (node && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      setSelectedBuilding(node.dataset.building);
    }
  });
  document.addEventListener('click', (event) => {
    const button = event.target.closest('[data-use-destination]');
    if (button) {
      $('#destinationSelect').value = button.dataset.useDestination;
      updateNodeSelections();
      const building = state.campus.buildings.find((entry) => entry.id === button.dataset.useDestination);
      toast(`${building ? building.name : 'Location'} set as destination.`);
    }
  });
}

async function initialize() {
  wireEvents();
  try {
    const health = await api('/api/health');
    setServerStatus('online', 'Local engine online');
    renderCampus(await api('/api/campus'));
    await loadDirectory();
    await findRoute();
    $('#searchInput').value = 'library';
    await searchBuildings();
  } catch (error) {
    setServerStatus('offline', 'Local engine unavailable');
    $('#campusMap').innerHTML = '<text x="420" y="245" text-anchor="middle" class="map-loading">Cannot connect to the C++ server. Run the build script first.</text>';
    toast(`Startup failed: ${error.message}. Check README.md.`, true);
  }
}

document.addEventListener('DOMContentLoaded', initialize);

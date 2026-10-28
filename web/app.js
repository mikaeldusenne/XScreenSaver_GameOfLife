"use strict";
const $ = (id) => document.getElementById(id);
const colors = ["#7de0bd", "#edbc77"];
const names = ["Menthe", "Or"];
const labels = ["Cellule", "Bloc", "Eater", "Glider", "Vaisseau", "Canon"];
const descriptions = [
  "Un petit changement peut transformer une collision. Dessinez plusieurs cellules avant de valider.",
  "Quatre cellules stables. Un obstacle économique, mais vulnérable aux collisions.",
  "Absorbe certains gliders avec la bonne orientation. Une défense spécialisée.",
  "Avance en diagonale : une case toutes les quatre générations. R pour viser le centre.",
  "Avance suivant un axe. Une attaque plus rapide, débloquée à 3 XP.",
  "Produit des gliders en continu. Prévoyez assez de place et orientez-le vers le centre.",
];
const errors = {
  "not-planning": "Attendez la prochaine phase de construction.",
  "invalid-placement": "Placement invalide.",
  "plan-full": "La limite est de 64 constructions par manche.",
  locked: "Cette figure se débloquera avec votre expérience.",
  energy: "Pas assez d’énergie disponible.",
  outside: "La figure dépasse de l’arène.",
  "home-only": "Construisez uniquement dans votre camp.",
  occupied: "Certaines cellules sont déjà occupées.",
  "room-not-found": "Cette partie n’existe plus. Créez-en une nouvelle.",
  "room-full": "Les deux places sont déjà prises.",
  "server-full": "Tous les salons sont occupés. Réessayez un peu plus tard.",
  "cannot-resume": "Cette place est déjà connectée, ou la reprise a expiré.",
  "waiting-player": "En attendant le retour de votre adversaire…",
  "stale-round": "La manche a changé. Recommencez votre placement.",
  "replay-disabled": "L’enregistrement est désactivé sur ce serveur.",
  "replay-unavailable": "La partie enregistrée est indisponible.",
  "replay-write-failed": "L’enregistrement a échoué. La partie a été arrêtée.",
  "cannot-create-room": "Le serveur ne peut pas créer de partie actuellement.",
};
let socket,
  catalog,
  state,
  seat = 0,
  room = "",
  token = "",
  seq = 0,
  selected = 3,
  transform = 0;
let cells = new Uint8Array(128 * 80),
  traces = new Uint8Array(cells.length),
  traceOwner = new Uint8Array(cells.length);
let pointer = null,
  drawPending = false,
  toastTimer,
  reconnectTimer,
  reconnects = 0,
  intent = null;
let replayFrames = null,
  replayIndex = 0,
  replayTimer = null,
  remainingAt = 0,
  maxPlanning = 20000;
const board = $("board"),
  ctx = board.getContext("2d");

function toast(text) {
  $("toast").textContent = text;
  $("toast").classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => $("toast").classList.remove("show"), 3500);
}
function stored(key) {
  try {
    return JSON.parse(sessionStorage.getItem(key));
  } catch {
    return null;
  }
}
function store(key, value) {
  try {
    sessionStorage.setItem(key, JSON.stringify(value));
  } catch {}
}
function connected() {
  return socket && socket.readyState === WebSocket.OPEN;
}
function canPlace() {
  return (
    !replayFrames &&
    connected() &&
    state?.phase === "planning" &&
    state.connected.every(Boolean) &&
    !state.ready[seat]
  );
}
function send(action, args = []) {
  if (!canPlace()) return;
  socket.send([action, ++seq, state.round, ...args].join(" "));
}
function connectionLabel(text, online) {
  $("connection").textContent = text;
  $("connection").classList.toggle("online", online);
}

function connect(request) {
  clearTimeout(reconnectTimer);
  if (socket) {
    socket.onclose = null;
    socket.close();
  }
  intent = request;
  connectionLabel("Connexion…", false);
  const ws = (socket = new WebSocket(
    `${location.protocol === "https:" ? "wss:" : "ws:"}//${location.host}/`,
    "life-duel-v1",
  ));
  ws.onmessage = async (event) => {
    if (ws !== socket) return;
    if (typeof event.data !== "string") {
      const url = URL.createObjectURL(event.data),
        link = document.createElement("a");
      link.href = url;
      link.download = `life-duel-${room}.duel`;
      link.click();
      setTimeout(() => URL.revokeObjectURL(url), 1000);
      return;
    }
    let m;
    try {
      m = JSON.parse(event.data);
    } catch {
      toast("Réponse du serveur illisible.");
      return;
    }
    if (m.type === "catalog") {
      if (m.rules !== 1) {
        toast("Version du serveur incompatible. Rechargez la page.");
        ws.onclose = null;
        ws.close();
        return;
      }
      catalog = m;
      maxPlanning = m.planningSeconds * 1000;
      buildPieces();
      ws.send(intent.join(" "));
    } else if (m.type === "welcome") {
      room = m.room;
      token = m.token;
      seat = m.player;
      seq = 0;
      reconnects = 0;
      store(`life-duel:${room}`, { token, seat });
      history.replaceState(null, "", `#${room}`);
      $("lobby").hidden = true;
      $("match").hidden = false;
      document.documentElement.style.setProperty("--accent", colors[seat]);
      transform = seat ? 2 : 0;
      connectionLabel("Connecté", true);
      updatePieces();
    } else if (m.type === "state") {
      receiveState(m);
    } else if (m.type === "error") {
      toast(errors[m.message] || "La commande a été refusée.");
      if (
        ["room-not-found", "cannot-resume", "room-full"].includes(m.message)
      ) {
        ws.onclose = null;
        ws.close();
        state = null;
        room = token = "";
        connectionLabel("Partie indisponible", false);
        $("lobby").hidden = false;
        $("match").hidden = true;
      }
    }
  };
  ws.onclose = () => {
    if (ws !== socket || replayFrames) return;
    connectionLabel("Déconnecté", false);
    updateUI();
    if (room && token) {
      reconnectTimer = setTimeout(
        () => connect(["resume", room, token]),
        Math.min(5000, 500 * 2 ** reconnects++),
      );
    } else toast("Connexion impossible. Vérifiez que le serveur est lancé.");
  };
  ws.onerror = () => connectionLabel("Connexion interrompue", false);
}

function receiveState(m) {
  const packed = atob(m.grid),
    next = new Uint8Array(cells.length);
  for (let i = 0; i < next.length; ++i)
    next[i] = (packed.charCodeAt(i >> 2) >> ((i & 3) * 2)) & 3;
  const elapsed = state ? Math.max(0, m.generation - state.generation) : 0;
  if (elapsed)
    for (let i = 0; i < cells.length; ++i) {
      traces[i] = Math.max(0, traces[i] - elapsed);
      if (cells[i] && !next[i]) {
        traces[i] = 9;
        traceOwner[i] = cells[i];
      }
      if (next[i]) traces[i] = 0;
    }
  cells = next;
  state = m;
  remainingAt = performance.now();
  maxPlanning = Math.max(maxPlanning, m.remaining);
  updateUI();
  requestDraw();
}

function points(piece, t) {
  return piece.cells.map(([x, y]) => {
    if (t & 4) x = piece.width - 1 - x;
    switch (t & 3) {
      case 1:
        return [piece.height - 1 - y, x];
      case 2:
        return [piece.width - 1 - x, piece.height - 1 - y];
      case 3:
        return [y, piece.width - 1 - x];
      default:
        return [x, y];
    }
  });
}

function buildPieces() {
  $("pieces").replaceChildren();
  catalog.pieces.forEach((piece, i) => {
    const button = document.createElement("button");
    button.className = "piece";
    button.dataset.piece = i;
    button.setAttribute("aria-label", `${labels[i]}, ${piece.cost} énergie`);
    const canvas = document.createElement("canvas");
    canvas.width = 108;
    canvas.height = 68;
    const label = document.createElement("span");
    label.className = "piece-name";
    label.textContent = labels[i];
    const cost = document.createElement("span");
    cost.className = "cost";
    cost.textContent = `${piece.cost} ◇`;
    const lock = document.createElement("span");
    lock.className = "lock";
    button.append(canvas, label, cost, lock);
    button.onclick = () => {
      selected = i;
      updatePieces();
      requestDraw();
    };
    $("pieces").append(button);
  });
  updatePieces();
}

function updatePieces() {
  if (!catalog) return;
  [...$("pieces").children].forEach((button, i) => {
    const p = catalog.pieces[i],
      locked = state && state.xp[seat] < p.unlock;
    button.disabled = !!locked || !!replayFrames;
    button.classList.toggle("selected", i === selected);
    button.setAttribute("aria-pressed", String(i === selected));
    button.querySelector(".lock").textContent = locked ? `${p.unlock} XP` : "";
    const c = button.querySelector("canvas"),
      g = c.getContext("2d");
    const shape = points(p, i === selected ? transform : 0);
    const w = Math.max(...shape.map((v) => v[0])) + 1,
      h = Math.max(...shape.map((v) => v[1])) + 1;
    const s = Math.min(11, 100 / w, 58 / h);
    g.clearRect(0, 0, c.width, c.height);
    g.fillStyle = colors[seat];
    g.globalAlpha = locked ? 0.4 : 0.9;
    for (const [x, y] of shape)
      g.fillRect(
        2 + x * s,
        (68 - h * s) / 2 + y * s,
        Math.max(1, s - 1),
        Math.max(1, s - 1),
      );
  });
  $("piece-description").textContent = descriptions[selected];
}

function updateUI() {
  if (!state) return;
  const paused =
    !replayFrames &&
    state.phase !== "waiting" &&
    state.phase !== "finished" &&
    (!connected() || !state.connected.every(Boolean));
  $("match-code").textContent = replayFrames
    ? "PARTIE ENREGISTRÉE"
    : `PARTIE ${room} · VOUS JOUEZ ${names[seat].toUpperCase()}`;
  const titles = {
    waiting: "Invitez votre adversaire.",
    planning: "À vous de construire.",
    running: "La vie suit son cours.",
    finished:
      state.winner === 3
        ? "Un monde partagé. Match nul."
        : `${names[state.winner - 1]} remporte le duel.`,
  };
  $("phase-title").textContent = paused
    ? "La partie est en pause."
    : replayFrames && state.phase !== "finished"
      ? "Revoir les générations."
      : titles[state.phase];
  $("notice").hidden = !paused && state.phase !== "waiting";
  $("notice").textContent = paused
    ? "Connexion interrompue. Reprise automatique pendant 30 secondes, puis victoire au joueur présent."
    : "Copiez le lien d’invitation et envoyez-le à votre ami. La partie commencera à son arrivée.";
  for (let p = 0; p < 2; ++p) {
    $(`score-${p}`).textContent = state.score[p];
    $(`bar-${p}`).style.width =
      `${Math.min(100, (state.score[p] / catalog.target) * 100)}%`;
    $(`you-${p}`).textContent = !replayFrames && p === seat ? "VOUS" : "";
  }
  [...$("zone-dots").children].forEach(
    (e, i) => (e.className = `held-${state.zones[i]}`),
  );
  $("energy").replaceChildren(
    document.createTextNode(`${state.energy[seat] - state.reserved} `),
    Object.assign(document.createElement("small"), { textContent: "/ 100" }),
  );
  $("xp").replaceChildren(
    document.createTextNode(`${state.xp[seat]} `),
    Object.assign(document.createElement("small"), { textContent: "XP" }),
  );
  $("plan-count").textContent = state.plans.length
    ? `${state.plans.length} construction${state.plans.length > 1 ? "s" : ""} · ${state.reserved} ◇`
    : "Aucune construction";
  $("ready").disabled = !canPlace();
  $("undo").disabled = !canPlace() || !state.plans.length;
  $("rotate").disabled = $("mirror").disabled = !canPlace();
  $("ready").textContent = state.ready[seat]
    ? "Plan validé · En attente…"
    : "Valider mes constructions ✓";
  $("download").hidden = state.phase !== "finished" || !!replayFrames;
  $("world-stats").textContent =
    `Génération ${state.generation} · ${state.population[0]} menthe / ${state.population[1]} or`;
  $("world-hint").textContent = replayFrames
    ? "Les mêmes placements, les mêmes conséquences."
    : canPlace()
      ? "Clic : poser · R : tourner · F : reflet"
      : "Surveillez les trois zones au centre.";
  const total = state.population[0] + state.population[1];
  const balance = total
    ? (state.population[0] - state.population[1]) / total
    : 0;
  // Global population affects only the ambient tint, never the victory score.
  const rgb = balance >= 0 ? [125, 224, 189] : [237, 188, 119],
    strength = Math.abs(balance) * 0.045;
  document.body.style.backgroundColor = `rgb(${[14, 23, 27].map((v, i) => Math.round(v + rgb[i] * strength)).join(",")})`;
  document.documentElement.style.setProperty(
    "--dominant",
    total && Math.abs(balance) > 0.04 ? rgb.join(" ") : "160 170 165",
  );
  updatePieces();
  updateClock();
}

function updateClock() {
  if (!state) return;
  const active = connected() && state.connected.every(Boolean) && !replayFrames;
  const remaining = Math.max(
    0,
    state.remaining - (active ? performance.now() - remainingAt : 0),
  );
  $("round-info").textContent =
    state.phase === "planning"
      ? `Manche ${state.round} / ${catalog.rounds} · ${Math.ceil(remaining / 1000)} s pour préparer`
      : state.phase === "running"
        ? `Manche ${state.round} / ${catalog.rounds} · 16 générations`
        : state.phase === "finished"
          ? "La partie est terminée."
          : "En attendant le second joueur.";
  $("countdown-bar").style.width =
    `${state.phase === "planning" ? Math.min(100, (remaining / maxPlanning) * 100) : 0}%`;
}
setInterval(updateClock, 200);

function requestDraw() {
  if (!drawPending) {
    drawPending = true;
    requestAnimationFrame(draw);
  }
}
function ghostValid(shape, x, y) {
  if (
    !canPlace() ||
    state.energy[seat] - state.reserved < catalog.pieces[selected].cost
  )
    return false;
  const occupied = new Set();
  for (const [p, px, py, t] of state.plans)
    for (const [dx, dy] of points(catalog.pieces[p], t))
      occupied.add((py + dy) * 128 + px + dx);
  return shape.every(([dx, dy]) => {
    const nx = x + dx,
      ny = y + dy;
    return (
      nx >= 0 &&
      ny >= 0 &&
      nx < 128 &&
      ny < 80 &&
      (seat ? nx >= 80 : nx < 48) &&
      !cells[ny * 128 + nx] &&
      !occupied.has(ny * 128 + nx)
    );
  });
}
function draw() {
  drawPending = false;
  const w = board.clientWidth || 1280,
    h = (w * 80) / 128,
    dpr = Math.min(devicePixelRatio || 1, 2);
  if (board.width !== Math.round(w * dpr)) {
    board.width = Math.round(w * dpr);
    board.height = Math.round(h * dpr);
  }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);
  const s = w / 128;
  ctx.fillStyle = "#7de0bd05";
  ctx.fillRect(0, 0, s * 48, h);
  ctx.fillStyle = "#edbc7705";
  ctx.fillRect(s * 80, 0, s * 48, h);
  ctx.strokeStyle = "#ffffff06";
  ctx.lineWidth = 0.5;
  ctx.beginPath();
  for (let x = 0; x <= 128; x += 4) {
    ctx.moveTo(x * s, 0);
    ctx.lineTo(x * s, h);
  }
  for (let y = 0; y <= 80; y += 4) {
    ctx.moveTo(0, y * s);
    ctx.lineTo(w, y * s);
  }
  ctx.stroke();
  for (let z = 0; z < 3; ++z) {
    const owner = state?.zones[z] || 0,
      x = 54 * s,
      y = (8 + z * 24) * s;
    ctx.fillStyle = owner ? `${colors[owner - 1]}0c` : "#ffffff03";
    ctx.fillRect(x, y, 20 * s, 16 * s);
    ctx.strokeStyle = owner ? `${colors[owner - 1]}88` : "#ffffff25";
    ctx.setLineDash([3, 5]);
    ctx.strokeRect(x, y, 20 * s, 16 * s);
    ctx.setLineDash([]);
    ctx.fillStyle = "#ffffff45";
    ctx.font = `${Math.max(8, s * 1.3)}px system-ui`;
    ctx.fillText(["Ⅰ", "Ⅱ", "Ⅲ"][z], x + 5, y + 13);
  }
  function cell(x, y, color, alpha = 1) {
    ctx.fillStyle = color;
    ctx.globalAlpha = alpha;
    const gap = s > 5 ? 0.8 : 0.3;
    if (s > 5 && ctx.roundRect) {
      ctx.beginPath();
      ctx.roundRect(
        x * s + gap / 2,
        y * s + gap / 2,
        s - gap,
        s - gap,
        s * 0.16,
      );
      ctx.fill();
    } else ctx.fillRect(x * s + gap / 2, y * s + gap / 2, s - gap, s - gap);
  }
  for (let i = 0; i < cells.length; ++i) {
    if (cells[i]) cell(i % 128, Math.floor(i / 128), colors[cells[i] - 1]);
    else if (traces[i])
      cell(
        i % 128,
        Math.floor(i / 128),
        colors[traceOwner[i] - 1],
        traces[i] / 45,
      );
  }
  if (state && catalog)
    for (const [p, x, y, t] of state.plans)
      for (const [dx, dy] of points(catalog.pieces[p], t))
        cell(x + dx, y + dy, colors[seat], 0.42);
  if (pointer && catalog && canPlace()) {
    const shape = points(catalog.pieces[selected], transform),
      ok = ghostValid(shape, pointer.x, pointer.y);
    for (const [dx, dy] of shape)
      cell(pointer.x + dx, pointer.y + dy, ok ? colors[seat] : "#f18d87", 0.65);
  }
  ctx.globalAlpha = 1;
}
new ResizeObserver(requestDraw).observe(board);
function point(event) {
  const r = board.getBoundingClientRect();
  return {
    x: Math.floor(((event.clientX - r.left) / r.width) * 128),
    y: Math.floor(((event.clientY - r.top) / r.height) * 80),
  };
}
board.onpointermove = (e) => {
  pointer = point(e);
  requestDraw();
};
board.onpointerleave = () => {
  pointer = null;
  requestDraw();
};
board.onpointerdown = (e) => {
  if (e.button !== 0) return;
  pointer = point(e);
  board.focus({ preventScroll: true });
  if (canPlace()) send("place", [selected, pointer.x, pointer.y, transform]);
  requestDraw();
};
$("rotate").onclick = () => {
  transform = (transform & 4) | ((transform + 1) & 3);
  updatePieces();
  requestDraw();
};
$("mirror").onclick = () => {
  transform ^= 4;
  updatePieces();
  requestDraw();
};
$("undo").onclick = () => send("undo");
$("ready").onclick = () => send("ready");
document.addEventListener("keydown", (e) => {
  if (e.target instanceof HTMLInputElement || $("rules").open || !canPlace())
    return;
  if (e.key.toLowerCase() === "r") $("rotate").click();
  else if (e.key.toLowerCase() === "f") $("mirror").click();
  else if (e.key === " ") {
    e.preventDefault();
    send("ready");
  } else if (e.key.toLowerCase() === "z" && (e.ctrlKey || e.metaKey)) {
    e.preventDefault();
    send("undo");
  } else if (e.target === board && e.key.startsWith("Arrow")) {
    e.preventDefault();
    pointer ||= { x: seat ? 88 : 40, y: 38 };
    pointer.x = Math.max(
      0,
      Math.min(
        127,
        pointer.x + (e.key === "ArrowRight") - (e.key === "ArrowLeft"),
      ),
    );
    pointer.y = Math.max(
      0,
      Math.min(79, pointer.y + (e.key === "ArrowDown") - (e.key === "ArrowUp")),
    );
    requestDraw();
  } else if (e.target === board && e.key === "Enter" && pointer) {
    e.preventDefault();
    send("place", [selected, pointer.x, pointer.y, transform]);
  }
});
$("create").onclick = () => connect(["create"]);
$("join-form").onsubmit = (e) => {
  e.preventDefault();
  connect(["join", $("room-code").value.trim().toUpperCase()]);
};
$("new-game").onclick = () => {
  clearInterval(replayTimer);
  location.href = location.pathname;
};
$("invite").onclick = async () => {
  const url = `${location.origin}${location.pathname}#${room}`;
  if (["localhost", "127.0.0.1", "[::1]"].includes(location.hostname)) {
    window.prompt(
      "Pour un autre appareil, remplacez localhost par l’adresse IP du serveur (lancé avec --bind 0.0.0.0). Pour deux onglets sur cette machine :",
      url,
    );
    return;
  }
  try {
    await navigator.clipboard.writeText(url);
    toast("Lien d’invitation copié.");
  } catch {
    window.prompt("Copiez ce lien pour inviter votre ami :", url);
  }
};
$("download").onclick = () => {
  if (connected()) socket.send("replay");
};
$("rules-button").onclick = () => $("rules").showModal();
$("close-rules").onclick = () => $("rules").close();
$("rules").onclick = (e) => {
  if (e.target === $("rules")) $("rules").close();
};

// Local playback of public match journals; live matches always use server states.
function evolve(a) {
  const b = new Uint8Array(a.length);
  for (let y = 0; y < 80; ++y)
    for (let x = 0; x < 128; ++x) {
      let n = 0,
        blue = 0;
      for (let dy = -1; dy <= 1; ++dy)
        for (let dx = -1; dx <= 1; ++dx) {
          const nx = x + dx,
            ny = y + dy;
          if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= 128 || ny >= 80)
            continue;
          const v = a[ny * 128 + nx];
          n += v !== 0;
          blue += v === 1;
        }
      const i = y * 128 + x;
      b[i] = a[i]
        ? n === 2 || n === 3
          ? a[i]
          : 0
        : n === 3
          ? blue >= 2
            ? 1
            : 2
          : 0;
    }
  return b;
}
async function loadReplay(file) {
  if (file.size > 131072) throw Error("Fichier trop volumineux.");
  const text = await file.text(),
    lines = text.trim().split("\n");
  if (lines.shift()?.trim() !== "duel1\t1\t128\t80")
    throw Error("Format de partie incompatible.");
  // Get the versioned catalogue without creating a room.
  if (!catalog)
    catalog = await new Promise((resolve, reject) => {
      const ws = new WebSocket(
        `${location.protocol === "https:" ? "wss:" : "ws:"}//${location.host}/`,
        "life-duel-v1",
      );
      const timeout = setTimeout(() => {
        ws.close();
        reject(Error("Serveur indisponible."));
      }, 5000);
      ws.onmessage = (e) => {
        const m = JSON.parse(e.data);
        if (m.type === "catalog") {
          clearTimeout(timeout);
          ws.close();
          resolve(m);
        }
      };
      ws.onerror = () => {
        clearTimeout(timeout);
        reject(Error("Serveur indisponible."));
      };
    });
  let grid = new Uint8Array(10240),
    generation = 0,
    round = 1,
    score = [0, 0],
    runs = 0,
    winner = 0;
  for (let p = 0; p < 2; ++p)
    for (let y = 39; y < 41; ++y)
      for (let x = 0; x < 2; ++x) grid[y * 128 + (p ? 84 : 42) + x] = p + 1;
  const frames = [];
  function capture() {
    const pop = [0, 0],
      zones = [0, 0, 0],
      held = [0, 0];
    for (const v of grid) if (v) pop[v - 1]++;
    for (let z = 0; z < 3; ++z) {
      const count = [0, 0];
      for (let y = 8 + z * 24; y < 24 + z * 24; ++y)
        for (let x = 54; x < 74; ++x) {
          const v = grid[y * 128 + x];
          if (v) count[v - 1]++;
        }
      if (count[0] >= 3 && count[0] > count[1]) zones[z] = 1;
      if (count[1] >= 3 && count[1] > count[0]) zones[z] = 2;
      if (zones[z]) held[zones[z] - 1]++;
    }
    return {
      grid: grid.slice(),
      generation,
      round,
      score: score.slice(),
      population: pop,
      zones,
      held,
    };
  }
  frames.push(capture());
  for (const raw of lines) {
    const v = raw.trim().split("\t"),
      numbers = v.slice(1).map(Number);
    if (v[0] === "turn") {
      if (
        !Number.isInteger(numbers[0]) ||
        numbers[0] < 1 ||
        numbers[0] > 30 ||
        numbers[1] !== generation
      )
        throw Error("Manche invalide.");
      round = numbers[0];
    } else if (v[0] === "place") {
      const [p, k, x, y, t] = numbers;
      if (
        v.length !== 6 ||
        numbers.some((n) => !Number.isInteger(n) || n < 0) ||
        p > 1 ||
        k >= catalog.pieces.length ||
        t > 7 ||
        x >= 128 ||
        y >= 80
      )
        throw Error("Placement invalide.");
      for (const [dx, dy] of points(catalog.pieces[k], t)) {
        if (x + dx >= 128 || y + dy >= 80)
          throw Error("Placement hors grille.");
        grid[(y + dy) * 128 + x + dx] = p + 1;
      }
    } else if (v[0] === "run") {
      if (++runs > 30) throw Error("Trop de manches.");
      frames.push(capture());
    } else if (v[0] === "check" || v[0] === "finish") {
      const target = numbers[0];
      if (
        !Number.isInteger(target) ||
        target < generation ||
        target > generation + 16 ||
        target > 480
      )
        throw Error("Génération invalide.");
      while (generation < target) {
        grid = evolve(grid);
        generation++;
        const f = capture();
        for (let p = 0; p < 2; ++p) if (f.held[p] >= 2) score[p]++;
        f.score = score.slice();
        frames.push(f);
      }
      if (v[0] === "finish") {
        winner = numbers[1];
        if (!Number.isInteger(winner) || winner < 1 || winner > 3)
          throw Error("Résultat invalide.");
      }
    } else throw Error("Ligne inconnue dans le journal.");
  }
  if (socket) {
    socket.onclose = null;
    socket.close();
  }
  clearTimeout(reconnectTimer);
  socket = null;
  frames.at(-1).winner = winner;
  replayFrames = frames;
  replayIndex = 0;
  seat = 0;
  traces.fill(0);
  buildPieces();
  $("lobby").hidden = true;
  $("match").hidden = false;
  $("toolbox").hidden = true;
  $("invite").hidden = true;
  $("replay-controls").hidden = false;
  $("replay-position").max = frames.length - 1;
  connectionLabel("Relecture locale", true);
  showReplay();
  startPlayback();
}
function showReplay() {
  const f = replayFrames[replayIndex];
  cells = f.grid;
  state = {
    ...f,
    phase: f.winner ? "finished" : "running",
    winner: f.winner || 0,
    energy: [0, 0],
    xp: [0, 0],
    reserved: 0,
    plans: [],
    connected: [true, true],
    ready: [false, false],
    remaining: 0,
  };
  $("replay-position").value = replayIndex;
  $("replay-label").textContent =
    `${f.generation} / ${replayFrames.at(-1).generation}`;
  updateUI();
  requestDraw();
}
function startPlayback() {
  clearInterval(replayTimer);
  $("replay-toggle").textContent = "Pause";
  replayTimer = setInterval(() => {
    if (replayIndex >= replayFrames.length - 1) {
      clearInterval(replayTimer);
      replayTimer = null;
      $("replay-toggle").textContent = "Rejouer";
      return;
    }
    replayIndex++;
    showReplay();
  }, 100);
}
$("replay-file").onchange = async (e) => {
  const file = e.target.files[0];
  if (file)
    try {
      await loadReplay(file);
    } catch (error) {
      toast(error.message);
    }
  e.target.value = "";
};
$("replay-toggle").onclick = () => {
  if (replayTimer) {
    clearInterval(replayTimer);
    replayTimer = null;
    $("replay-toggle").textContent = "Reprendre";
  } else {
    if (replayIndex >= replayFrames.length - 1) replayIndex = 0;
    startPlayback();
  }
};
$("replay-position").oninput = (e) => {
  replayIndex = Number(e.target.value);
  showReplay();
};

const intro = $("intro-canvas").getContext("2d");
for (let p = 0; p < 2; ++p) {
  intro.fillStyle = colors[p];
  const seed = [
    [1, 0],
    [2, 1],
    [0, 2],
    [1, 2],
    [2, 2],
  ];
  for (let k = 0; k < 4; ++k)
    for (const [x, y] of seed) {
      intro.globalAlpha = 0.2 + k * 0.18;
      const u = p ? 39 - x - k * 4 : 2 + x + k * 4,
        v = 3 + y + k;
      intro.fillRect(u * 10, v * 10, 7, 7);
    }
}
const invited = location.hash.slice(1).toUpperCase();
if (/^[A-F0-9]{8}$/.test(invited)) {
  $("room-code").value = invited;
  const saved = stored(`life-duel:${invited}`);
  connect(saved?.token ? ["resume", invited, saved.token] : ["join", invited]);
}

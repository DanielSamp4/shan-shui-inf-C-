(() => {
  const canvas = document.querySelector('#stroke');
  const ctx = canvas.getContext('2d');
  const stages = [...document.querySelectorAll('.stage')];
  const inputs = { width: document.querySelector('#width'), noise: document.querySelector('#noise'), edge: document.querySelector('#edge') };
  const out = { width: document.querySelector('#width-out'), noise: document.querySelector('#noise-out'), edge: document.querySelector('#edge-out'), geometry: document.querySelector('#geometry'), result: document.querySelector('#result'), caption: document.querySelector('#caption') };
  const captions = ['A linha central é a intenção do gesto. Sozinha, ela ainda não carrega tinta.', 'A normal de cada ponto abre a linha em duas bordas. A largura cresce no meio e recua nas pontas.', 'Uma perturbação leve na largura quebra a regularidade matemática. O gesto passa a parecer depositado por uma ferramenta.', 'O contorno final reforça a borda do polígono. No SVG ele é um stroke; no C++ é mais uma cobertura calculada pelo rasterizador.'];
  let stage = 0, seed = 7;
  function random() { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; }
  function fit() { const ratio = Math.min(devicePixelRatio || 1, 2), box = canvas.getBoundingClientRect(); canvas.width = Math.round(box.width * ratio); canvas.height = Math.round(box.height * ratio); ctx.setTransform(ratio, 0, 0, ratio, 0, 0); return box; }
  function draw() {
    const { width: w, height: h } = fit(), baseline = h * .53, maxWidth = +inputs.width.value, noise = +inputs.noise.value / 100, edge = +inputs.edge.value / 10, points = [], count = 65;
    for (let i = 0; i <= count; i++) { const p = i / count; points.push({ p, x: w * .08 + p * w * .84, y: baseline + Math.sin(p * Math.PI * 2.4) * h * .07 + Math.sin(p * Math.PI * 5.7) * h * .022 + (random() - .5) * h * .018 }); }
    const paper = ctx.createLinearGradient(0, 0, w, h); paper.addColorStop(0, '#f0eade'); paper.addColorStop(1, '#ddd4c2'); ctx.fillStyle = paper; ctx.fillRect(0, 0, w, h);
    for (let i = 0; i < 450; i++) { const shade = 95 + random() * 30; ctx.fillStyle = `rgba(${shade},${shade - 8},${shade - 19},.025)`; ctx.fillRect(random() * w, random() * h, 1, 1); }
    ctx.strokeStyle = 'rgba(146,75,61,.7)'; ctx.lineWidth = 1; ctx.setLineDash([4, 4]); ctx.beginPath(); points.forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y)); ctx.stroke(); ctx.setLineDash([]);
    if (stage >= 1) {
      const left = [], right = [];
      points.forEach((p, i) => { const before = points[Math.max(0, i - 1)], after = points[Math.min(count, i + 1)], normal = Math.atan2(after.y - before.y, after.x - before.x) + Math.PI / 2, taper = Math.pow(Math.sin(p.p * Math.PI), .58), width = maxWidth * taper * (1 + (random() - .5) * noise * .75); left.push([p.x + Math.cos(normal) * width, p.y + Math.sin(normal) * width]); right.push([p.x - Math.cos(normal) * width, p.y - Math.sin(normal) * width]); });
      ctx.globalCompositeOperation = 'multiply'; ctx.fillStyle = 'rgba(42,49,42,.46)'; ctx.beginPath(); left.forEach((p, i) => i ? ctx.lineTo(...p) : ctx.moveTo(...p)); right.slice().reverse().forEach(p => ctx.lineTo(...p)); ctx.closePath(); ctx.fill();
      if (stage >= 2) { ctx.save(); ctx.clip(); for (let i = 0; i < 100; i++) { const px = w * (.1 + random() * .8), py = baseline - h * .1 + random() * h * .2; ctx.strokeStyle = `rgba(30,35,30,${.03 + random() * .09})`; ctx.lineWidth = .4 + random() * .6; ctx.beginPath(); ctx.moveTo(px, py); ctx.lineTo(px + 8 + random() * 20, py + 2 + random() * 8); ctx.stroke(); } ctx.restore(); }
      if (stage >= 3) { ctx.globalCompositeOperation = 'multiply'; ctx.strokeStyle = `rgba(25,30,26,${.48 + edge * .12})`; ctx.lineWidth = edge; ctx.lineJoin = 'round'; ctx.beginPath(); left.forEach((p, i) => i ? ctx.lineTo(...p) : ctx.moveTo(...p)); right.slice().reverse().forEach(p => ctx.lineTo(...p)); ctx.closePath(); ctx.stroke(); }
    }
    ctx.globalCompositeOperation = 'source-over'; out.width.textContent = maxWidth; out.noise.textContent = Math.round(noise * 100); out.edge.textContent = edge.toFixed(1); out.geometry.textContent = stage ? 'polígono' : 'linha'; out.result.textContent = ['esqueleto', 'silhueta', 'tinta irregular', 'pincelada'][stage];
  }
  function activate(index) { stage = index; stages.forEach((button, i) => button.classList.toggle('active', i === index)); out.caption.textContent = captions[index]; draw(); }
  Object.values(inputs).forEach(input => input.addEventListener('input', draw)); stages.forEach(button => button.addEventListener('click', () => activate(+button.dataset.stage))); document.querySelector('#new-line').addEventListener('click', () => { seed = (Math.random() * 0xffffffff) >>> 0; draw(); }); addEventListener('resize', draw); draw();
})();

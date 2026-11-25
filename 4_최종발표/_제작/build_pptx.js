// 최종 PPT — 장마다 SVG를 그린 PNG 한 장 + 영상 · 음원 + 발표자 노트
//   python3 make_svg.py && node svg2png_2x.js && node build_pptx.js
const pptxgen = require("pptxgenjs");
const fs = require("fs"), path = require("path");
const D = __dirname, TOP = path.join(D, "..");
const OUT = path.join(TOP, "32203482이종하_개인맞춤형디바이스개발_최종발표.pptx");
const px = (v) => v / 192;                      // 1920px = 10in
const b64 = (f) => "image/" + (f.endsWith(".png") ? "png" : "jpeg") + ";base64," + fs.readFileSync(f).toString("base64");

function scripts() {
  const txt = fs.readFileSync(path.join(TOP, "발표_구성.txt"), "utf8"), out = {};
  for (const b of txt.split(/\n(?=\d+ -{6,})/)) {
    const m = b.match(/^(\d+) -{6,}/); if (!m) continue;
    const lines = b.split("\n"), i = lines.findIndex((l) => l.startsWith("대본:")); if (i < 0) continue;
    const said = [lines[i].replace(/^대본:\s*/, "")];
    for (let k = i + 1; k < lines.length && /^\s{4,}\S/.test(lines[k]); k++) said.push(lines[k].trim());
    out[Number(m[1])] = said.join("\n");
  }
  return out;
}

// 장 번호 → 넣을 것 (좌표는 1920×1080 px, make_svg.py 와 같은 자리)
const A = path.join(D, "assets"), M = path.join(D, "media"), V = path.join(TOP, "영상"), S = path.join(TOP, "음원");
const MEDIA = {
  9: [{ type: "video", path: path.join(M, "중간_걷기_카메라.mp4"), cover: path.join(M, "중간_걷기_표지.jpg"), x: 131, y: 350, w: 810, h: 492 },
      { type: "video", path: path.join(M, "중간_동결흉내_카메라.mp4"), cover: path.join(M, "중간_동결흉내_표지.jpg"), x: 977, y: 350, w: 810, h: 492 }],
  12: ["기본_110bpm", "성자의행진_110bpm", "작은별_110bpm", "환희의송가_110bpm"].map((n, i) =>
      ({ type: "audio", path: path.join(S, n + ".mp3"), cover: path.join(A, "speaker.png"), x: 690, y: 360 + i * 165 - 62, w: 84, h: 84 })),
  14: [{ type: "video", path: path.join(V, "앱화면_15초_커서.mp4"), cover: path.join(A, "앱영상_첫장면.jpg"), x: 330, y: 265, w: 1260, h: 709 }],
  15: [{ type: "video", path: path.join(V, "보행동결.mp4"), cover: path.join(A, "시연_보행동결.jpg"), x: 133, y: 300, w: 1654, h: 604 }],
  16: [{ type: "video", path: path.join(V, "넘어짐.mp4"), cover: path.join(A, "시연_넘어짐.jpg"), x: 133, y: 300, w: 1654, h: 630 }],
};

(async () => {
  const pres = new pptxgen();
  pres.layout = "LAYOUT_16x9";
  pres.title = "보행동결 자극 시스템 · 최종발표"; pres.author = "이종하";
  const notes = scripts();
  const files = fs.readdirSync(path.join(D, "png")).filter((f) => f.endsWith(".png")).sort();
  for (const f of files) {
    const n = Number(f.slice(0, 2));
    const s = pres.addSlide();
    s.addImage({ path: path.join(D, "png", f), x: 0, y: 0, w: 10, h: 5.625, altText: f.replace(".png", "").slice(3).replace(/_/g, " ") });
    for (const m of MEDIA[n] || []) {
      const o = { type: m.type, path: m.path, x: px(m.x), y: px(m.y), w: px(m.w), h: px(m.h) };
      if (m.cover) o.cover = b64(m.cover);
      s.addMedia(o);
    }
    if (notes[n]) s.addNotes(notes[n]);
  }
  await pres.writeFile({ fileName: OUT });
  console.log("만듦:", OUT, files.length + "장");
})();

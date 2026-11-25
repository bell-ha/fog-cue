// 슬라이드 SVG → 3840×2160 PNG — node svg2png_2x.js
const { chromium } = require("/private/tmp/claude-501/-Users-jongha-Desktop-GitHub-fog-cue/50b92f47-547a-4427-8d93-174205b4cb72/scratchpad/video/node_modules/playwright-core");
const fs = require("fs"), path = require("path");
(async () => {
  const inD = path.join(__dirname, "..", "슬라이드"), outD = path.join(__dirname, "png");
  fs.rmSync(outD, { recursive: true, force: true }); fs.mkdirSync(outD);
  const b = await chromium.launch({ executablePath: "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" });
  const p = await (await b.newContext({ viewport: { width: 1920, height: 1080 }, deviceScaleFactor: 2 })).newPage();
  for (const f of fs.readdirSync(inD).filter((f) => f.endsWith(".svg")).sort()) {
    await p.goto("file://" + path.join(inD, f)); await p.waitForTimeout(200);
    await p.screenshot({ path: path.join(outD, f.replace(".svg", ".png")) });
  }
  await b.close();
})();

// 将母版 assets/keep-awake-icon-design.svg 渲染为多尺寸 PNG，
// 并打包成 Windows ICO（PNG 内嵌条目，自 Vista 起支持）。
//
// 运行：npm run icon   （或 node assets/render_icon.mjs）
import { Resvg } from '@resvg/resvg-js';
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const svg = readFileSync(join(here, 'keep-awake-icon-design.svg'), 'utf8');

const SIZES = [16, 24, 32, 48, 64, 256];

function renderPng(size) {
  const resvg = new Resvg(svg, { fitTo: { mode: 'width', value: size } });
  return resvg.render().asPng();
}

function buildIco(entries) {
  // entries: [{ size, png }]
  const count = entries.length;
  const header = Buffer.alloc(6);
  header.writeUInt16LE(0, 0); // reserved
  header.writeUInt16LE(1, 2); // type: icon
  header.writeUInt16LE(count, 4);

  const parts = [header];
  let offset = 6 + 16 * count;
  for (const { size, png } of entries) {
    const e = Buffer.alloc(16);
    e.writeUInt8(size >= 256 ? 0 : size, 0); // width（256 用 0 表示）
    e.writeUInt8(size >= 256 ? 0 : size, 1); // height
    e.writeUInt8(0, 2); // 调色板颜色数
    e.writeUInt8(0, 3); // 保留
    e.writeUInt16LE(1, 4); // 色彩平面
    e.writeUInt16LE(32, 6); // 位深度
    e.writeUInt32LE(png.length, 8); // 数据字节数
    e.writeUInt32LE(offset, 12); // 数据偏移
    parts.push(e);
    offset += png.length;
  }
  for (const { png } of entries) parts.push(png);
  return Buffer.concat(parts);
}

const rendered = SIZES.map((s) => ({ size: s, png: renderPng(s) }));

writeFileSync(join(here, 'tray.ico'), buildIco(rendered));
writeFileSync(join(here, 'tray_preview.png'), rendered.find((r) => r.size === 256).png);

console.log('generated tray.ico (%s) and tray_preview.png', SIZES.join(', '));

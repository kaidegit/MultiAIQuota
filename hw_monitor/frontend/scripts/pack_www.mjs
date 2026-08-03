// Pack the built Svelte frontend (www/) into a single gzip-compressed tar
// archive (www.tar.gz) that is embedded into the firmware. The device
// decompresses it into the PSRAM littlefs at boot.
//
// Format: ustar tar + gzip (level 9). Both macOS Keka and the ESP32 ROM
// miniz (tinfl) can decompress it.
import { readdirSync, readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { gzipSync } from 'node:zlib';

const wwwDir = path.resolve(import.meta.dirname, '../../www');
const outFile = path.resolve(import.meta.dirname, '../../www.tar.gz');

// Collect all regular files under dir, relative to base ('' for www root).
function collectFiles(dir, base, out) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    if (entry.name === '.DS_Store') continue;
    const full = path.join(dir, entry.name);
    const rel = base ? `${base}/${entry.name}` : entry.name;
    if (entry.isDirectory()) {
      collectFiles(full, rel, out);
    } else if (entry.isFile()) {
      out.push(rel);
    }
  }
}

// Build a 512-byte ustar header for a regular file.
function tarHeader(name, size) {
  const buf = Buffer.alloc(512);
  buf.write(name, 0, 100, 'utf8');
  buf.write('0000644\0', 100, 8); // mode
  buf.write('0000000\0', 108, 8); // uid
  buf.write('0000000\0', 116, 8); // gid
  buf.write(`${size.toString(8).padStart(11, '0')}\0`, 124, 12); // size
  buf.write('00000000000\0', 136, 12); // mtime
  buf.write('0', 156, 1); // typeflag: regular file
  buf.write('ustar\0', 257, 6); // magic
  buf.write('00', 263, 2); // version
  let sum = 0;
  for (let i = 0; i < 512; i++) sum += buf[i];
  // POSIX: checksum is computed with the checksum field treated as spaces.
  sum += 8 * 0x20;
  buf.write(`${sum.toString(8).padStart(6, '0')}\0 `, 148, 8); // chksum
  return buf;
}

const files = [];
collectFiles(wwwDir, '', files);
files.sort();

const chunks = [];
for (const rel of files) {
  const data = readFileSync(path.join(wwwDir, rel));
  chunks.push(tarHeader(rel, data.length));
  chunks.push(data);
  const pad = (512 - (data.length % 512)) % 512;
  if (pad) chunks.push(Buffer.alloc(pad));
}
chunks.push(Buffer.alloc(1024)); // end-of-archive marker

const tar = Buffer.concat(chunks);
const gz = gzipSync(tar, { level: 9 });
writeFileSync(outFile, gz);
console.log(`www.tar.gz: ${gz.length} bytes (${files.length} files, tar payload ${tar.length} bytes)`);

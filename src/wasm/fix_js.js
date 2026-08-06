const fs = require('fs');
const file = process.argv[2];
let data = fs.readFileSync(file, 'utf8');
data = data.replace(/UTF8Decoder\.decode\(heapOrArray\.subarray\(idx, endPtr\)\)/g, "UTF8Decoder.decode(heapOrArray.buffer.resizable ? heapOrArray.slice(idx, endPtr) : heapOrArray.subarray(idx, endPtr))");
data = data.replace(/UTF8Decoder\.decode\(HEAPU8\.subarray\(ptr, end\)\)/g, "UTF8Decoder.decode(HEAPU8.buffer.resizable ? HEAPU8.slice(ptr, end) : HEAPU8.subarray(ptr, end))");
data = data.replace(/UTF8Decoder\.decode\(heapOrArray\.buffer \? heapOrArray\.subarray\(idx, endPtr\) : new Uint8Array\(heapOrArray\.slice\(idx, endPtr\)\)\)/g, "UTF8Decoder.decode(heapOrArray.buffer ? (heapOrArray.buffer.resizable ? heapOrArray.slice(idx, endPtr) : heapOrArray.subarray(idx, endPtr)) : new Uint8Array(heapOrArray.slice(idx, endPtr)))");

// Fix Emscripten O_SEARCH issue in mayOpen
data = data.replace(
  /if \(FS\.isDir\(node\.mode\)\) {\s*\/\/ opening for write\s*\/\/ TODO: check for O_SEARCH\? \(== search for dir only\)\s*if \(mode !== 'r' \|\| \(flags & \(512 \| 64\)\)\) {\s*return 31;\s*}\s*}/g,
  "if (FS.isDir(node.mode)) { if (flags & 2097152) mode = 'x'; /* opening for write */ if ((mode !== 'r' && mode !== 'x') || (flags & (512 | 64))) { return 31; } }"
);

fs.writeFileSync(file, data, 'utf8');

const fs = require('fs');
const file = process.argv[2];
let data = fs.readFileSync(file, 'utf8');
data = data.replace(/UTF8Decoder\.decode\(heapOrArray\.subarray\(idx, endPtr\)\)/g, "UTF8Decoder.decode(heapOrArray.buffer.resizable ? heapOrArray.slice(idx, endPtr) : heapOrArray.subarray(idx, endPtr))");
data = data.replace(/UTF8Decoder\.decode\(HEAPU8\.subarray\(ptr, end\)\)/g, "UTF8Decoder.decode(HEAPU8.buffer.resizable ? HEAPU8.slice(ptr, end) : HEAPU8.subarray(ptr, end))");
data = data.replace(/UTF8Decoder\.decode\(heapOrArray\.buffer \? heapOrArray\.subarray\(idx, endPtr\) : new Uint8Array\(heapOrArray\.slice\(idx, endPtr\)\)\)/g, "UTF8Decoder.decode(heapOrArray.buffer ? (heapOrArray.buffer.resizable ? heapOrArray.slice(idx, endPtr) : heapOrArray.subarray(idx, endPtr)) : new Uint8Array(heapOrArray.slice(idx, endPtr)))");
fs.writeFileSync(file, data, 'utf8');

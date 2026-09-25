// SPDX-License-Identifier: CC0-1.0
// Writes source DSF, MSB reference and PCM WAV to the supplied output directory.
const fs = require('node:fs');
const path = require('node:path');
const out = process.argv[2];
if (!out) throw new Error('Usage: node generate.cjs <output-directory>');
fs.mkdirSync(out, {recursive: true});
const n = 16384, b = Buffer.alloc(92+n*2), raw = Buffer.alloc(n*2);
b.write('DSD ');b.writeBigUInt64LE(28n,4);b.writeBigUInt64LE(BigInt(b.length),12);
b.write('fmt ',28);b.writeBigUInt64LE(52n,32);b.writeUInt32LE(1,40);
b.writeUInt32LE(2,48);b.writeUInt32LE(2,52);b.writeUInt32LE(2822400,56);
b.writeUInt32LE(1,60);b.writeBigUInt64LE(BigInt(n*8),64);b.writeUInt32LE(4096,72);
b.write('data',80);b.writeBigUInt64LE(BigInt(12+n*2),84);
const state = [{a:0,b:0,y:0},{a:0,b:0,y:0}];
for(let i=0;i<n;i++)for(let c=0;c<2;c++) {
    let v=0;const s=state[c];
    for(let k=0;k<8;k++) {
        const x=.45*Math.sin((i*8+k)*(c?733:997)*2*Math.PI/2822400);
        s.a+=x-s.y;s.b+=s.a-s.y;s.y=s.b>=0?1:-1;
        if(s.y>0)v|=1<<k;
    }
    b[92+Math.floor(i/4096)*8192+c*4096+i%4096]=v;
    let r=0;for(let k=0;k<8;k++)r|=((v>>k)&1)<<(7-k);
    raw[i*2+c]=r;
}
fs.writeFileSync(path.join(out,'pattern.dsf'),b);
fs.writeFileSync(path.join(out,'pattern.raw'),raw);
const wav=Buffer.alloc(44+4096*4);
wav.write('RIFF');wav.writeUInt32LE(wav.length-8,4);wav.write('WAVEfmt ',8);
wav.writeUInt32LE(16,16);wav.writeUInt16LE(1,20);wav.writeUInt16LE(2,22);
wav.writeUInt32LE(44100,24);wav.writeUInt32LE(176400,28);
wav.writeUInt16LE(4,32);wav.writeUInt16LE(16,34);wav.write('data',36);
wav.writeUInt32LE(wav.length-44,40);
for(let i=0;i<4096;i++)for(let c=0;c<2;c++)
    wav.writeInt16LE(((i*29+c*101)&65535)-32768,44+(i*2+c)*2);
fs.writeFileSync(path.join(out,'pattern-pcm.wav'),wav);

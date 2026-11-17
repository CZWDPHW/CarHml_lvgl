// Use a caller-supplied lv_font_conv installation; never modify that installation.
const path = require('path');
const converter = path.resolve(process.argv[2]);
const bold = process.argv[3] === 'bold';
const cjkBold = process.argv[3] === 'cjk-bold';
const ft = require(path.join(path.dirname(converter), 'lib/freetype'));
if (bold || cjkBold) {
    let faceIndex = 0;
    const createFace = ft.fontface_create;
    ft.fontface_create = function(data, size) {
        const face = createFace(data, size);
        face.size = size;
        face.synthesize = bold || faceIndex++ > 0;
        return face;
    };
    const render = ft.glyph_render;
    ft.glyph_render = function(face, code, options) {
        const glyph = render(face, code, options);
        if (!face.synthesize) return glyph;
        if (!glyph.width || !glyph.height) return glyph;
        // QML can request bold from a family that only supplies a regular face.
        // Precompute a small horizontal emboldening, keeping runtime drawing static.
        const strength = Math.max(1, Math.round(face.size / 24) || 1);
        glyph.pixels = glyph.pixels.map(row => Array.from(
            {length: glyph.width + strength}, (_, x) => {
                let value = 0;
                for (let dx = 0; dx <= strength; ++dx) value = Math.max(value, row[x - dx] || 0);
                return value;
            }));
        glyph.width += strength;
        glyph.advance_x += strength;
        return glyph;
    };
}
require(path.join(path.dirname(converter), 'lib/cli')).run(process.argv.slice(4))
    .catch(error => { console.error(error); process.exit(1); });

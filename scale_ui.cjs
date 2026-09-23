const fs = require('fs');
let css = fs.readFileSync('frontend/src/cyberpunk.css', 'utf8');

css = css.replace(/font-size:\s*(\d+)px/g, (match, p1) => {
    let size = parseInt(p1, 10);
    if (size <= 10) size += 3;
    else if (size <= 14) size += 4;
    else if (size <= 18) size += 4;
    else if (size <= 25) size += 4;
    return 'font-size: ' + size + 'px';
});

css = css.replace(/width:\s*196px/g, 'width: 280px');
css = css.replace(/margin-left:\s*196px/g, 'margin-left: 280px');
css = css.replace(/calc\(100%\s*-\s*196px\)/g, 'calc(100% - 280px)');
css = css.replace(/padding:\s*24px 25px 0/g, 'padding: 40px 50px 0');
css = css.replace(/gap:\s*18px/g, 'gap: 24px');

fs.writeFileSync('frontend/src/cyberpunk.css', css);
console.log('Scaled UI for desktop');

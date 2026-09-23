const fs = require('fs');
let css = fs.readFileSync('frontend/src/styles.css', 'utf8');

// Scale font-size: 10px;
css = css.replace(/font-size:\s*(\d+)px/g, (match, p1) => {
    let size = parseInt(p1, 10);
    if (size <= 7) size += 4;
    else if (size <= 10) size += 4;
    else if (size <= 14) size += 5;
    else if (size <= 18) size += 5;
    else if (size <= 25) size += 5;
    return 'font-size: ' + size + 'px';
});

// Scale font: 10px var(--mono); or font: 10px/1.5 var(--mono);
css = css.replace(/font:\s*(\d+)px/g, (match, p1) => {
    let size = parseInt(p1, 10);
    if (size <= 7) size += 5;
    else if (size <= 10) size += 5;
    else if (size <= 14) size += 5;
    return 'font: ' + size + 'px';
});

fs.writeFileSync('frontend/src/styles.css', css);
console.log('Scaled styles.css');

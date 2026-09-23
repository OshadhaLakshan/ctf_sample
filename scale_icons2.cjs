const fs = require('fs');
let tsx = fs.readFileSync('frontend/src/components.tsx', 'utf8');

tsx = tsx.replace(/size=\{([0-9]+)\}/g, (match, p1) => {
    let size = parseInt(p1, 10);
    if (size <= 10) size += 4;
    else if (size <= 14) size += 4;
    else if (size <= 18) size += 4;
    else if (size <= 24) size += 4;
    return 'size={' + size + '}';
});

fs.writeFileSync('frontend/src/components.tsx', tsx);
console.log('Scaled components.tsx icons');

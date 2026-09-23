const fs = require('fs');
let css = fs.readFileSync('frontend/src/cyberpunk.css', 'utf8');

css = css.replace(/grid-template-columns: minmax\(310px, 0\.78fr\) minmax\(430px, 1\.22fr\)/g, 'grid-template-columns: minmax(450px, 1fr) minmax(600px, 1.3fr)');

fs.writeFileSync('frontend/src/cyberpunk.css', css);

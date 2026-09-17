/** Browser regression checks use a real running backend; no responses are mocked. */
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || '../frontend/node_modules/playwright'); // Portable Playwright module override.
const assert = require('node:assert/strict'); // Built-in assertions avoid a separate test runner dependency.
const fs = require('node:fs'); // Writes only local QA artifacts.
const path = require('node:path'); // Resolves repository-relative screenshot paths.

/** Exercises the console, editor, approval workflow, evidence, navigation, and phone layout. */
async function main() {
  const root = path.resolve(__dirname, '..'); // Project root used for screenshots and reports.
  const browser = await chromium.launch({ headless: true, ...(process.env.ROWDOGG_BROWSER ? { channel: process.env.ROWDOGG_BROWSER } : {}) }); // Clean temporary browser profile.
  const context = await browser.newContext({ viewport: { width: 1440, height: 1100 }, reducedMotion: 'reduce' }); // Desktop QA dimensions.
  const page = await context.newPage(); // Isolated application tab.
  const errors = []; // Collect JavaScript failures separately from expected network shutdown messages.
  const checks = []; // Human-readable passed checks written at completion.
  page.on('pageerror', error => errors.push(error.message));
  try {
    await page.goto(process.env.ROWDOGG_URL || 'http://127.0.0.1:8080', { waitUntil: 'networkidle' });
    await page.getByText('SYSTEM ONLINE', { exact: true }).waitFor();
    await page.screenshot({ path: path.join(root, 'tmp', 'desktop-ready.png'), fullPage: true });
    checks.push('Real C++ health and desktop initial rendering');
    await page.getByRole('button', { name: 'Run challenge', exact: false }).click();
    await page.getByText('EXECUTION RESULT', { exact: true }).waitFor();
    await page.locator('.answer').filter({ hasText: 'A → C → B → D → E → F' }).waitFor();
    assert.equal(await page.locator('.result-cost strong').textContent(), '13');
    await page.getByText('LIVE', { exact: true }).waitFor();
    assert.equal(await page.locator('.check-list .pass').count(), 5);
    checks.push('Dijkstra result, path highlight, independent checks, live WebSocket');
    await page.getByRole('button', { name: 'Re-verify' }).click();
    await page.getByRole('status').filter({ hasText: 'Re-verification passed' }).waitFor();
    checks.push('Reverification endpoint');
    const download = page.waitForEvent('download'); // Export must generate a real audit file.
    await page.getByRole('button', { name: 'Export full report' }).click();
    assert.match((await download).suggestedFilename(), /^rowdogg-.*\.json$/);
    checks.push('Full report download');
    await page.getByRole('button', { name: 'Dismiss notification' }).click();
    await page.evaluate(() => window.scrollTo(0, 0));
    await page.screenshot({ path: path.join(root, 'tmp', 'desktop-result.png'), fullPage: true });
    await page.getByRole('button', { name: 'Evidence vault', exact: true }).click();
    assert.ok(await page.locator('.vault-record').count() >= 4);
    await page.getByText('Inspect raw payload').first().click();
    checks.push('Persistent evidence vault and payload inspection');
    await page.getByRole('button', { name: 'Challenge lab', exact: true }).click();
    assert.equal(await page.locator('.library-card').count(), 6);
    await page.locator('.library-card').filter({ hasText: 'Signal in the noise' }).getByRole('button', { name: 'Open challenge' }).click();
    await page.getByLabel('CTF-IR JSON').fill('{broken');
    await page.getByRole('button', { name: 'Launch operation' }).click();
    await page.getByRole('alert').waitFor();
    assert.equal(await page.getByRole('dialog').count(), 1);
    await page.getByRole('button', { name: 'Close challenge editor' }).click();
    checks.push('Invalid JSON is actionable and never silently submitted');
    await page.locator('.library-card').filter({ hasText: 'The hidden transmission' }).getByRole('button', { name: 'Open challenge' }).click();
    await page.getByLabel('APPROVAL POLICY').selectOption('manual');
    await page.getByRole('button', { name: 'Launch operation' }).click();
    for (let step = 0; step < 3; step++) { // Each action requires an individual approval.
      await page.getByRole('button', { name: 'Approve this action' }).waitFor();
      if (step === 0) await page.screenshot({ path: path.join(root, 'tmp', 'manual-approval.png'), fullPage: true });
      await page.getByRole('button', { name: 'Approve this action' }).click();
      await page.waitForFunction(expected => document.querySelector('.operation-chips')?.textContent.includes(`${expected}/12 steps`), step + 1);
    }
    await page.locator('.answer').filter({ hasText: 'CTF{evidence_over_assumptions}' }).waitFor();
    checks.push('Manual HTTP / Base64 / flag approval with actual fixture evidence');
    await page.getByRole('button', { name: 'Operations', exact: true }).click();
    await page.getByRole('button', { name: 'Checks passed', exact: true }).click();
    assert.ok(await page.locator('tbody tr').count() >= 2);
    await page.getByLabel('Search operations').fill('Ghost route');
    assert.ok(await page.locator('tbody tr').count() >= 1);
    assert.ok((await page.locator('tbody').innerText()).includes('Ghost route'));
    checks.push('History search and verified-check filter');
    await page.getByRole('button', { name: 'System', exact: true }).click();
    assert.equal(await page.locator('.capabilities>span').count(), 22);
    checks.push('Actual solver registry and topology');
    if ((await (await page.request.get(new URL('/api/v1/health', page.url()).href)).json()).model_available) {
      await page.getByRole('button', { name: 'Test inference', exact: true }).click();
      await page.locator('.connection-feedback.success').waitFor({ timeout: 190000 });
      await page.getByRole('button', { name: 'Overview', exact: true }).click();
      await page.getByLabel('CTF PROBLEM STATEMENT').fill('Decode the Base64 string SGVsbG8= and return the plaintext.');
      await page.getByRole('button', { name: 'Interpret & solve' }).click();
      await page.locator('.solution-explanation strong').filter({ hasText: 'Question cross-check passed' }).waitFor({ timeout: 190000 });
      assert.equal(await page.locator('.answer').textContent(), 'Hello');
      await page.locator('.full-result summary').click();
      assert.ok((await page.locator('.full-result pre').innerText()).includes('base64_decode'));
      assert.match(await page.locator('body').evaluate(element => getComputedStyle(element).cursor), /cursor.svg/);
      checks.push('Actual Gemma diagnostic, pasted challenge, answer cross-check, inspectable result, custom cursor');
      await page.screenshot({ path: path.join(root, 'tmp', 'gemma-browser-result.png'), fullPage: true });
    }
    await page.setViewportSize({ width: 390, height: 844 });
    await page.getByRole('button', { name: 'Toggle navigation' }).click();
    await page.getByRole('button', { name: 'Overview', exact: true }).click();
    if (await page.getByRole('button', { name: 'Dismiss notification' }).isVisible()) await page.getByRole('button', { name: 'Dismiss notification' }).click();
    await page.evaluate(() => window.scrollTo(0, 0));
    await page.screenshot({ path: path.join(root, 'tmp', 'mobile-result.png'), fullPage: true });
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), 'Phone layout must not overflow horizontally');
    await page.getByRole('button', { name: 'New operation' }).click();
    await page.screenshot({ path: path.join(root, 'tmp', 'mobile-editor.png'), fullPage: true });
    assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), 'Phone editor must not overflow horizontally');
    await page.keyboard.press('Escape');
    assert.equal(await page.getByRole('dialog').count(), 0);
    checks.push('390px phone layout, editor, keyboard dismissal, no horizontal overflow');
    assert.deepEqual(errors, []);
    checks.push('No browser JavaScript errors');
    fs.writeFileSync(path.join(root, 'tmp', 'browser-report.json'), JSON.stringify({ passed: checks.length, checks, errors }, null, 2));
    console.log(`${checks.length} browser workflow checks passed`);
  } finally {
    await browser.close();
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; }); // Fail CI on any browser regression.

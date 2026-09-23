import { useEffect, useState } from 'react';
import {
  Check,
  ChevronRight,
  Copy,
  Cpu,
  LoaderCircle,
  RefreshCw,
  Save,
  Terminal,
} from 'lucide-react';
import { api } from './api';
import type { ModelConfig, ModelStatus } from './types';

/** Configures and tests the real local model service without pretending HTTP health proves inference. */
export function ModelConnection({ onUpdate }: { onUpdate: () => void }) {
  const [status, setStatus] = useState<ModelStatus | null>(null); // Live transport/model discovery result.
  const [config, setConfig] = useState<ModelConfig>({
    base_url: 'http://127.0.0.1:8081',
    model: '',
    timeout_seconds: 180,
    max_tokens: 4096,
  }); // Editable non-secret connection settings.
  const [busy, setBusy] = useState(''); // Exact in-flight operation, used to prevent duplicate requests.
  const [message, setMessage] = useState(''); // Save/test feedback with actual server errors.
  const [passed, setPassed] = useState(false); // True only after an explicit inference test succeeds.
  const command = '.\\scripts\\start-llama.ps1 -ModelPath .\\models\\gemma-4-E2B-it-Q4_0.gguf'; // Reproducible launcher for a locally downloaded model.

  /** Refreshes server health and optionally reloads saved form fields. */
  async function refresh(load = false) {
    setBusy('probe');
    setMessage('');
    try {
      const report = await api<ModelStatus>('/model');
      setStatus(report);
      if (load) setConfig(report.config);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Could not reach the API.');
    } finally {
      setBusy('');
    }
  }
  useEffect(() => {
    void refresh(true);
  }, []);
  /** Saves the validated endpoint, rediscovers models, and invalidates stale test results. */
  async function save(event: React.FormEvent) {
    event.preventDefault();
    setBusy('save');
    setPassed(false);
    try {
      const report = await api<ModelStatus>('/model', config);
      setStatus(report);
      setConfig(report.config);
      setMessage('Connection settings saved. ' + report.message);
      onUpdate();
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Save failed.');
    } finally {
      setBusy('');
    }
  }
  /** Performs schema-constrained generation on the actual loaded model. */
  async function test() {
    setBusy('test');
    setPassed(false);
    setMessage('Generating a diagnostic JSON response. CPU inference can take a moment…');
    try {
      const result = await api<{ passed: boolean; elapsed_ms: number; message: string }>(
        '/model/test',
        {},
      );
      setPassed(result.passed);
      setMessage(`${result.message} ${(result.elapsed_ms / 1000).toFixed(1)} seconds.`);
      onUpdate();
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Inference test failed.');
    } finally {
      setBusy('');
    }
  }
  /** Copies the literal setup command without executing any shell from the web application. */
  async function copy() {
    try {
      await navigator.clipboard.writeText(command);
      setMessage('Startup command copied. Run it in PowerShell from this project directory.');
    } catch {
      setMessage('Select and copy the command below.');
    }
  }

  return (
    <section className="panel model-connection">
      <div className="panel-heading">
        <h2>
          <Cpu size={19} />
          Model connection
        </h2>
        <span className={`connection-state ${status?.available ? 'ready' : ''}`}>
          <i />
          {status?.state || 'CHECKING'}
        </span>
      </div>
      <div className="model-body">
        <div className="model-diagnostic">
          <span className="eyebrow">LLAMA.CPP / LOCAL INFERENCE</span>
          <h3>
            {status?.available
              ? 'Runtime connected.'
              : status?.state === 'loading'
                ? 'Model is loading.'
                : 'Bring your model online.'}
          </h3>
          <p>{status?.message || 'Checking the model server…'}</p>
          {status?.selected_model && <code>{status.selected_model}</code>}
        </div>
        <form onSubmit={save}>
          <label>
            SERVER ADDRESS
            <input
              value={config.base_url}
              onChange={(event) => setConfig({ ...config, base_url: event.target.value })}
              placeholder="http://127.0.0.1:8081"
              required
            />
          </label>
          <label>
            MODEL ID <span className="muted">Leave blank to auto-detect</span>
            <input
              list="model-names"
              value={config.model}
              onChange={(event) => setConfig({ ...config, model: event.target.value })}
              placeholder="Automatically use the loaded model"
            />
            <datalist id="model-names">
              {status?.models.map((model) => (
                <option key={model} value={model} />
              ))}
            </datalist>
          </label>
          <div className="form-row">
            <label>
              TIMEOUT / SECONDS
              <input
                type="number"
                min={10}
                max={600}
                value={config.timeout_seconds}
                onChange={(event) =>
                  setConfig({ ...config, timeout_seconds: Number(event.target.value) })
                }
              />
            </label>
            <label>
              OUTPUT TOKEN LIMIT
              <input
                type="number"
                min={256}
                max={16384}
                step={256}
                value={config.max_tokens}
                onChange={(event) =>
                  setConfig({ ...config, max_tokens: Number(event.target.value) })
                }
              />
            </label>
          </div>
          <div className="connection-actions">
            <button className="primary" type="submit" disabled={!!busy}>
              <Save size={15} />
              Save connection
            </button>
            <button
              className="secondary"
              type="button"
              disabled={!!busy}
              onClick={() => void refresh()}
            >
              <RefreshCw size={15} />
              Check server
            </button>
            <button
              className="secondary"
              type="button"
              disabled={!!busy || !status?.available}
              onClick={() => void test()}
            >
              {busy === 'test' ? (
                <LoaderCircle className="spin" size={15} />
              ) : (
                <Terminal size={15} />
              )}
              Test inference
            </button>
          </div>
        </form>
        {message && (
          <div
            className={passed ? 'connection-feedback success' : 'connection-feedback'}
            role="status"
          >
            {passed && <Check size={16} />}
            <span>{message}</span>
          </div>
        )}
        <details className="setup-guide" open={!status?.available}>
          <summary>
            <ChevronRight size={16} />
            First-time setup
          </summary>
          <ol>
            <li>
              Install the runtime: <code>winget install llama.cpp</code>
            </li>
            <li>
              Download a compatible Gemma 4 GGUF into the <code>models</code> folder.
            </li>
            <li>
              From this project, run the command below. It binds the model to port 8081, separate
              from the app.
            </li>
            <li>
              Wait for the model to load, then choose <strong>Check server</strong> and{' '}
              <strong>Test inference</strong>.
            </li>
          </ol>
          <div className="setup-command">
            <code>{command}</code>
            <button
              className="icon-button"
              type="button"
              onClick={() => void copy()}
              aria-label="Copy llama startup command"
            >
              <Copy size={16} />
            </button>
          </div>
          <p>
            HTTP 503 means the model is still loading. For a protected server, set{' '}
            <code>ROWDOGG_LLAMA_API_KEY</code> before starting the C++ backend. Model weights are
            separate from the runtime.
          </p>
        </details>
      </div>
    </section>
  );
}

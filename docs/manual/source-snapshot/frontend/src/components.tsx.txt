import { useEffect, useRef, useState } from 'react';
import {
  ArrowDownToLine,
  ArrowUpRight,
  Check,
  CheckCheck,
  ChevronRight,
  Circle,
  Copy,
  FileJson,
  LoaderCircle,
  Play,
  ShieldCheck,
  Upload,
  X,
} from 'lucide-react';
import { examples } from './examples';
import { downloadJson } from './api';
import type { Example, Run, Spec, Verification } from './types';

/** Maps engine states to clear labels without implying semantic review occurred. */
export function statusLabel(status: string): string {
  return (
    (
      {
        computed: 'Checks passed',
        verified: 'Verified',
        evidence_verified: 'Evidence verified',
        awaiting_approval: 'Awaiting approval',
        needs_input: 'Needs input',
        needs_review: 'Needs review',
        running: 'Running',
        failed: 'Failed',
        rejected: 'Rejected',
        stopped: 'Stopped',
        queued: 'Queued',
      } as Record<string, string>
    )[status] || status
  );
}

/** A consistent text-and-color status marker used across views. */
export function Badge({ status }: { status: string }) {
  return (
    <span className={`badge ${status}`}>
      <span />
      {statusLabel(status)}
    </span>
  );
}

/** Renders graph structure from real CTF-IR and highlights only returned path edges. */
export function Graph({ spec, path = [] }: { spec: Spec; path?: number[] }) {
  const count = Number(spec.input.nodes || 0); // Validated backend node count, capped visually below.
  const nodes = Array.from({ length: Math.min(count, 16) }, (_, index) => ({
    x: 250 + Math.cos((index / Math.min(count, 16)) * Math.PI * 2 - Math.PI) * 190,
    y: 130 + Math.sin((index / Math.min(count, 16)) * Math.PI * 2 - Math.PI) * 90,
  })); // Radial node coordinates.
  const edges = (spec.input.edges || []) as number[][]; // Weighted graph edges from the structured input.
  return (
    <div className="graph-wrap">
      <div className="graph-caption">
        <span>
          <span className="tiny-dot" />
          NETWORK TOPOLOGY
        </span>
        <span>
          {count} NODES / {edges.length} LINKS
        </span>
      </div>
      <svg
        viewBox="0 0 500 260"
        role="img"
        aria-label="Challenge network topology; highlighted edges show the computed path"
      >
        <defs>
          <pattern id="graph-grid" width="20" height="20" patternUnits="userSpaceOnUse">
            <circle cx="1" cy="1" r=".7" fill="#3c4444" />
          </pattern>
        </defs>
        <rect width="500" height="260" fill="url(#graph-grid)" />
        {edges.map(([from, to, weight], index) => {
          // Skip off-canvas vertices for large graphs while preserving the underlying input.
          if (!nodes[from] || !nodes[to]) return null;
          const active = path.some(
            (node, position) =>
              position > 0 &&
              ((path[position - 1] === from && node === to) ||
                (!spec.input.directed && path[position - 1] === to && node === from)),
          ); // Real solution membership.
          return (
            <g key={index} className={active ? 'edge active' : 'edge'}>
              <line x1={nodes[from].x} y1={nodes[from].y} x2={nodes[to].x} y2={nodes[to].y} />
              <rect
                x={(nodes[from].x + nodes[to].x) / 2 - 10}
                y={(nodes[from].y + nodes[to].y) / 2 - 9}
                width="20"
                height="18"
                rx="3"
              />
              <text x={(nodes[from].x + nodes[to].x) / 2} y={(nodes[from].y + nodes[to].y) / 2 + 4}>
                {weight}
              </text>
            </g>
          );
        })}
        {nodes.map((node, index) => (
          <g key={index} className={path.includes(index) ? 'node active' : 'node'}>
            <circle cx={node.x} cy={node.y} r="18" />
            <text x={node.x} y={node.y + 5}>
              {(spec.input.labels as string[] | undefined)?.[index] ||
                String.fromCharCode(65 + index)}
            </text>
          </g>
        ))}
      </svg>
      <div className="graph-legend">
        <span>
          <i />
          Available link
        </span>
        <span>
          <i className="lime" />
          Computed path
        </span>
        {count > 16 && <span>Showing first 16 nodes</span>}
      </div>
    </div>
  );
}

/** Decorative orbital schematic uses native SVG rather than an external asset. */
export function Orbital() {
  return (
    <svg className="orbital" viewBox="0 0 380 290" aria-hidden="true">
      <defs>
        <radialGradient id="halo">
          <stop offset="0" stopColor="#bcff00" stopOpacity=".12" />
          <stop offset="1" stopColor="#bcff00" stopOpacity="0" />
        </radialGradient>
      </defs>
      <circle cx="190" cy="145" r="144" fill="url(#halo)" />
      <g fill="none" stroke="#b9f523" strokeOpacity=".25">
        <circle cx="190" cy="145" r="100" />
        <ellipse cx="190" cy="145" rx="100" ry="36" transform="rotate(-25 190 145)" />
        <ellipse cx="190" cy="145" rx="100" ry="65" transform="rotate(-25 190 145)" />
        <ellipse cx="190" cy="145" rx="35" ry="100" transform="rotate(-25 190 145)" />
        <ellipse cx="190" cy="145" rx="67" ry="100" transform="rotate(-25 190 145)" />
        <ellipse
          cx="190"
          cy="145"
          rx="142"
          ry="43"
          transform="rotate(-25 190 145)"
          strokeOpacity=".6"
        />
        <path d="M23 145h334M190 10v270" strokeDasharray="3 6" />
      </g>
      <g fill="#c3ff35">
        <circle cx="65" cy="195" r="4" />
        <circle cx="284" cy="101" r="4" />
        <circle cx="190" cy="45" r="3" />
      </g>
      <g fill="#859675" fontSize="9" fontFamily="monospace">
        <text x="270" y="35">
          LOCAL SYSTEM
        </text>
        <text x="15" y="253">
          51° 30′ N / 00° 07′ E
        </text>
        <text x="278" y="252">
          NO UPLINK
        </text>
      </g>
      <path
        d="M162 128h27l13 13-13 13h-9l23 20h-23l-18-18v-28zm14 11v7h14l4-4-4-3h-14z"
        fill="#c3ff35"
        transform="translate(-1 -4)"
      />
    </svg>
  );
}

/** Displays independent checks and semantic review as separate levels of assurance. */
export function VerificationPanel({ run, onVerify }: { run: Run | null; onVerify: () => void }) {
  const verification = run?.verification; // Actual stored check results; null means not executed.
  const passed = verification?.checks.filter((check) => check.passed).length || 0; // Count for the completion meter.
  return (
    <section className="panel verification-panel">
      <div className="panel-heading">
        <h2>
          <ShieldCheck size={21} />
          Verification
        </h2>
        <span className="micro">INDEPENDENT ENGINE</span>
      </div>
      <div className={`verification-emblem ${verification?.passed ? 'complete' : ''}`}>
        <ShieldCheck size={30} />
      </div>
      <h3>
        {verification
          ? verification.passed
            ? 'Evidence over assumptions.'
            : 'Verification needs attention.'
          : 'Trust is earned.'}
      </h3>
      <p className="muted verification-intro">
        {verification
          ? verification.method
          : 'Every result is checked by the C++ engine before it earns a verified status.'}
      </p>
      <div className="check-list">
        {verification
          ? verification.checks.map((check) => (
              <div key={check.name}>
                <span className={check.passed ? 'check-icon' : 'failed-icon'}>
                  {check.passed ? <Check size={18} /> : <X size={18} />}
                </span>
                <span>{check.name.replaceAll('_', ' ')}</span>
                <span className={check.passed ? 'pass' : 'fail'}>
                  {check.passed ? 'PASS' : 'FAIL'}
                </span>
              </div>
            ))
          : [
              'Schema validation',
              'Structural correctness',
              'Computational checks',
              'Semantic review',
            ].map((label, index) => (
              <div key={label}>
                <span className="check-pending">0{index + 1}</span>
                <span>{label}</span>
                <Circle size={14} className="muted" />
              </div>
            ))}
      </div>
      <div className="semantic-box">
        <span className="micro">GEMMA SEMANTIC REVIEW</span>
        <p>
          {run?.semantic_review.status === 'completed'
            ? run.semantic_review.answers_question && run.semantic_review.input_matches_question
              ? 'Result answers the original question.'
              : 'Result needs semantic review.'
            : run?.semantic_review.reason || 'Runs after deterministic checks pass.'}
        </p>
        {run?.semantic_review.status === 'completed' && (
          <p className="review-reason">{run.semantic_review.reason}</p>
        )}
        {run?.semantic_review.confidence !== undefined && (
          <span className="micro">
            MODEL CONFIDENCE · {Math.round(run.semantic_review.confidence * 100)}%{' '}
            <span className="muted">(self-reported)</span>
          </span>
        )}
      </div>
      <div className="verify-footer">
        <span>
          {verification
            ? `${passed}/${verification.checks.length} CHECKS PASSED`
            : 'AWAITING EXECUTION'}
        </span>
        {run?.spec && run.result && (
          <button className="text-button" onClick={onVerify}>
            Re-verify <ArrowUpRight size={18} />
          </button>
        )}
      </div>
    </section>
  );
}

/** Shows the result with precise verification semantics and an auditable JSON export. */
export function Result({ run, notify }: { run: Run; notify: (message: string) => void }) {
  const result = run.result; // Result values always originate in the actual engine response.
  if (!result) return null;
  const answer =
    result.flag ??
    result.text ??
    (result.path
      ? (result.path as number[])
          .map(
            (node) =>
              (run.spec?.input.labels as string[] | undefined)?.[node] ||
              String.fromCharCode(65 + node),
          )
          .join(' → ')
      : (result.network ?? result.symbolic ?? null)); // Prefer compact human-readable output.
  /** Copies the exact answer, reporting clipboard permission failures. */
  async function copy() {
    try {
      await navigator.clipboard.writeText(
        answer !== null ? String(answer) : JSON.stringify(result, null, 2),
      );
      notify('Result copied to clipboard.');
    } catch {
      notify('Clipboard is unavailable. Use Export to save the result.');
    }
  }
  return (
    <div className="result-box">
      <div className="result-heading">
        <span className="micro">
          <CheckCheck size={19} />
          EXECUTION RESULT
        </span>
        <button className="icon-button" onClick={copy} aria-label="Copy result">
          <Copy size={19} />
        </button>
      </div>
      {answer !== null && <div className="answer">{String(answer)}</div>}
      {result.cost !== undefined && (
        <div className="result-cost">
          TOTAL COST <strong>{String(result.cost ?? 'Unreachable')}</strong>
        </div>
      )}
      {result.challenge_acceptance === 'not_checked' && (
        <p className="muted">
          Flag format and evidence checked. Challenge scoreboard acceptance has not been checked.
        </p>
      )}
      {answer === null && <pre>{JSON.stringify(result, null, 2)}</pre>}
      {run.solution && (
        <div className="solution-explanation">
          <strong>
            {run.solution.question_crosschecked
              ? 'Question cross-check passed'
              : 'Question cross-check incomplete'}
          </strong>
          <p>{run.solution.explanation}</p>
        </div>
      )}
      {answer !== null && (
        <details className="full-result">
          <summary>Full result & intermediate steps</summary>
          <pre>{JSON.stringify(result, null, 2)}</pre>
        </details>
      )}
      <button
        className="text-button"
        onClick={() => downloadJson(`rowdogg-${run.id.slice(0, 8)}.json`, run)}
      >
        Export full report <ArrowDownToLine size={18} />
      </button>
    </div>
  );
}

/** Modal challenge editor supports structured inputs, natural language, and bounded action plans. */
export function ChallengeModal({
  initial,
  onClose,
  onSubmit,
  busy,
}: {
  initial: Example;
  onClose: () => void;
  onSubmit: (request: unknown) => Promise<void>;
  busy: boolean;
}) {
  const [example, setExample] = useState(initial); // Selected library fixture, editable below.
  const [title, setTitle] = useState(initial.title); // Human-readable operation title.
  const [question, setQuestion] = useState(initial.description); // Challenge statement, treated as data.
  const [mode, setMode] = useState(initial.actions ? 'agent' : 'solver'); // Pipeline selection.
  const [inputMode, setInputMode] = useState(
    initial.spec || initial.actions ? 'structured' : 'natural',
  ); // Model-free structured input or Gemma interpretation.
  const [content, setContent] = useState(
    JSON.stringify(initial.spec || initial.actions || {}, null, 2),
  ); // Editable JSON source.
  const [approval, setApproval] = useState('assisted'); // Action approval policy.
  const [steps, setSteps] = useState(12); // Hard agent action limit.
  const [error, setError] = useState(''); // Local parse/upload/submit error.
  const dialog = useRef<HTMLDivElement>(null); // Focus trap boundary.
  const upload = useRef<HTMLInputElement>(null); // Hidden text/JSON file input.
  useEffect(() => {
    const previous = document.activeElement as HTMLElement | null; // Restore focus when modal closes.
    dialog.current?.querySelector<HTMLInputElement>('input')?.focus();
    /** Keeps keyboard focus inside the dialog and supports Escape dismissal. */
    function handleKey(event: KeyboardEvent) {
      if (event.key === 'Escape' && !busy) onClose();
      if (event.key === 'Tab') {
        const controls = dialog.current?.querySelectorAll<HTMLElement>(
          'button:not(:disabled), input, textarea, select',
        );
        if (!controls?.length) return;
        const first = controls[0],
          last = controls[controls.length - 1];
        if (event.shiftKey && document.activeElement === first) {
          event.preventDefault();
          last.focus();
        } else if (!event.shiftKey && document.activeElement === last) {
          event.preventDefault();
          first.focus();
        }
      }
    }
    document.addEventListener('keydown', handleKey);
    document.body.style.overflow = 'hidden';
    return () => {
      document.removeEventListener('keydown', handleKey);
      document.body.style.overflow = '';
      previous?.focus();
    };
  }, [busy, onClose]);
  /** Loads a complete example while retaining the operator's approval preference. */
  function choose(value: string) {
    const selected = examples.find((item) => item.id === value)!;
    setExample(selected);
    setTitle(selected.title);
    setQuestion(selected.description);
    setMode(selected.actions ? 'agent' : 'solver');
    setContent(JSON.stringify(selected.spec || selected.actions, null, 2));
    setInputMode('structured');
    setError('');
  }
  /** Imports only bounded text or JSON as challenge data. */
  async function readFile(file?: File) {
    if (!file) return;
    if (file.size > 131072) {
      setError('Files must be smaller than 128 KiB.');
      return;
    }
    try {
      const text = await file.text();
      if (file.name.endsWith('.json')) {
        const value = JSON.parse(text);
        setContent(JSON.stringify(value, null, 2));
        setMode(Array.isArray(value) ? 'agent' : 'solver');
        setInputMode('structured');
      } else {
        setQuestion(text);
        setInputMode('natural');
      }
      setError('');
    } catch {
      setError('The file could not be read as valid text or JSON.');
    }
  }
  /** Parses local JSON before submitting the challenge to authoritative C++ validation. */
  async function submit(event: React.FormEvent) {
    event.preventDefault();
    setError('');
    try {
      const payload = {
        title,
        question,
        mode,
        approval,
        max_steps: steps,
        ...(inputMode === 'structured'
          ? mode === 'solver'
            ? { spec: JSON.parse(content) }
            : { actions: JSON.parse(content) }
          : {}),
      };
      await onSubmit(payload);
    } catch (failure) {
      setError(failure instanceof Error ? failure.message : 'Challenge submission failed.');
    }
  }
  return (
    <div
      className="modal-backdrop"
      onMouseDown={(event) => {
        if (event.target === event.currentTarget && !busy) onClose();
      }}
    >
      <div
        className="challenge-modal"
        role="dialog"
        aria-modal="true"
        aria-labelledby="new-title"
        ref={dialog}
      >
        <div className="modal-heading">
          <div>
            <span className="eyebrow">INITIALIZE / NEW OPERATION</span>
            <h2 id="new-title">Enter the challenge.</h2>
          </div>
          <button
            className="icon-button"
            onClick={onClose}
            disabled={busy}
            aria-label="Close challenge editor"
          >
            <X size={24} />
          </button>
        </div>
        <form onSubmit={submit}>
          <div className="form-row">
            <label>
              OPERATION NAME
              <input
                value={title}
                onChange={(event) => setTitle(event.target.value)}
                required
                maxLength={120}
              />
            </label>
            <label>
              LOAD EXAMPLE
              <select value={example.id} onChange={(event) => choose(event.target.value)}>
                {example.id === 'custom' && <option value="custom">Custom challenge</option>}
                {examples.map((item) => (
                  <option key={item.id} value={item.id}>
                    {item.title} / {item.tag}
                  </option>
                ))}
              </select>
            </label>
          </div>
          <div className="mode-select">
            <button
              type="button"
              className={mode === 'solver' ? 'selected' : ''}
              onClick={() => {
                setMode('solver');
                setInputMode('natural');
              }}
            >
              <FileJson size={22} />
              <span>
                Deterministic solver<small>MODE A / COMPUTE + VERIFY</small>
              </span>
            </button>
            <button
              type="button"
              className={mode === 'agent' ? 'selected' : ''}
              onClick={() => {
                setMode('agent');
                setInputMode('natural');
              }}
            >
              <Play size={22} />
              <span>
                Interactive agent<small>MODE B / PLAN + EXECUTE</small>
              </span>
            </button>
          </div>
          <label>
            CHALLENGE DESCRIPTION
            <textarea
              className="question-field"
              value={question}
              onChange={(event) => setQuestion(event.target.value)}
              placeholder="Describe the challenge and the expected result…"
              maxLength={65536}
            />
          </label>
          <div className="editor-toolbar">
            <div className="segmented">
              <button
                type="button"
                className={inputMode === 'structured' ? 'selected' : ''}
                onClick={() => setInputMode('structured')}
              >
                {mode === 'solver' ? 'CTF-IR' : 'Action plan'}
              </button>
              <button
                type="button"
                className={inputMode === 'natural' ? 'selected' : ''}
                onClick={() => setInputMode('natural')}
              >
                Local Gemma
              </button>
            </div>
            <button type="button" className="text-button" onClick={() => upload.current?.click()}>
              <Upload size={18} />
              Import .txt / .json
            </button>
            <input
              className="visually-hidden"
              ref={upload}
              type="file"
              accept=".txt,.json"
              aria-label="Import challenge file"
              onChange={(event) => void readFile(event.target.files?.[0])}
            />
          </div>
          {inputMode === 'structured' ? (
            <textarea
              className="code-editor"
              aria-label={mode === 'solver' ? 'CTF-IR JSON' : 'Action plan JSON'}
              spellCheck={false}
              value={content}
              onChange={(event) => setContent(event.target.value)}
            />
          ) : (
            <div className="model-notice">
              <span className="tiny-dot amber" />
              <p>
                The local model will{' '}
                {mode === 'solver'
                  ? 'translate your description into CTF-IR'
                  : 'plan one policy-checked action at a time'}
                . Start llama.cpp on port 8081 before running.
              </p>
            </div>
          )}
          {mode === 'agent' && (
            <div className="form-row">
              <label>
                APPROVAL POLICY
                <select value={approval} onChange={(event) => setApproval(event.target.value)}>
                  <option value="manual">Manual — approve every action</option>
                  <option value="assisted">Assisted — approve custom HTTP headers</option>
                  <option value="autonomous">Autonomous — loopback lab only</option>
                </select>
              </label>
              <label>
                MAXIMUM STEPS
                <input
                  type="number"
                  min={1}
                  max={32}
                  value={steps}
                  onChange={(event) => setSteps(Number(event.target.value))}
                />
              </label>
            </div>
          )}
          {error && (
            <div className="error-box" role="alert">
              {error}
            </div>
          )}
          <div className="modal-footer">
            <span>
              <ShieldCheck size={19} />
              Validated by the C++ policy engine
            </span>
            <button className="primary" disabled={busy} type="submit">
              {busy ? <LoaderCircle className="spin" size={20} /> : <Play size={19} />}{' '}
              {busy ? 'Initializing…' : 'Launch operation'}
              <ChevronRight size={20} />
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}

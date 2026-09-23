import { useState } from 'react';
import {
  ArrowRight,
  Braces,
  CheckCheck,
  ClipboardPaste,
  Cpu,
  FileCheck2,
  LoaderCircle,
  Sparkles,
  X,
} from 'lucide-react';
import type { Health, Run } from './types';

/** The primary user flow: paste a CTF statement and follow interpretation, execution and review. */
export function ProblemComposer({
  health,
  busy,
  run,
  onLaunch,
  onConnect,
}: {
  health: Health | null;
  busy: boolean;
  run: Run | null;
  onLaunch: (value: unknown) => Promise<void>;
  onConnect: () => void;
}) {
  const [question, setQuestion] = useState(() => {
    try {
      return localStorage.getItem('rowdogg-draft') || '';
    } catch {
      return '';
    }
  }); // Local recoverable draft, never sent until explicit submission.
  const [error, setError] = useState(''); // Actionable submission or clipboard failure.
  const [title, setTitle] = useState(''); // Optional custom title; the backend receives a readable default.
  const stages = [
    { id: 'interpret', title: 'Interpret', detail: 'Gemma → CTF-IR', icon: Braces },
    { id: 'execute', title: 'Solve', detail: 'Deterministic C++', icon: Cpu },
    { id: 'verify', title: 'Verify', detail: 'Independent checks', icon: FileCheck2 },
    { id: 'review', title: 'Cross-check', detail: 'Question ↔ answer', icon: CheckCheck },
  ]; // Visible architecture without requiring knowledge of JSON.
  /** Saves an unsent draft locally so navigation or a refresh does not discard pasted work. */
  function update(value: string) {
    setQuestion(value);
    try {
      localStorage.setItem('rowdogg-draft', value);
    } catch {
      /* Storage-disabled browsers still support editing and submission. */
    }
  }
  /** Reads the clipboard only after the operator explicitly requests it. */
  async function paste() {
    try {
      update(await navigator.clipboard.readText());
      setError('');
    } catch {
      setError('Use Ctrl+V in the problem field; your browser blocked clipboard access.');
    }
  }
  /** Starts the fully model-interpreted solve path, without silently inserting an example spec. */
  async function submit(event: React.FormEvent) {
    event.preventDefault();
    if (!question.trim()) return;
    setError('');
    try {
      await onLaunch({
        title: title.trim() || 'Pasted CTF challenge',
        question: question.trim(),
        mode: 'solver',
        approval: 'assisted',
        max_steps: 12,
      });
    } catch (failure) {
      setError(failure instanceof Error ? failure.message : 'Unable to start the challenge.');
    }
  }
  return (
    <section className="problem-composer">
      <div className="composer-title">
        <div>
          <span className="eyebrow">01 / INPUT_TERMINAL</span>
          <h2>
            CRACK THE
            <br />
            <span>CHALLENGE.</span>
          </h2>
          <p>
            Bring the challenge text. Gemma extracts the inputs. C++ computes and verifies. Gemma
            cross-checks the answer against your original question.
          </p>
        </div>
        <div className="composer-sigil" aria-hidden="true">
          <span>R</span>
          <i />
          <i />
          <b>
            LOCAL
            <br />
            NEURAL LINK
          </b>
        </div>
      </div>
      <form onSubmit={submit}>
        <label className="composer-label" htmlFor="pasted-problem">
          CTF PROBLEM STATEMENT <span>{question.length.toLocaleString()} / 65,536</span>
        </label>
        <textarea
          id="pasted-problem"
          value={question}
          onChange={(event) => update(event.target.value)}
          maxLength={65536}
          placeholder={
            'Paste the full challenge here, including encoded strings, graph edges, keys, constraints, and the expected flag format.\n\nExample: Decode the Base64 signal Q1RGe3RydXN0X2J1dF92ZXJpZnl9 and return the plaintext.'
          }
          required
        />
        <div className="composer-tools">
          <button type="button" className="text-button" onClick={() => void paste()}>
            <ClipboardPaste size={15} />
            Paste from clipboard
          </button>
          <button
            type="button"
            className="text-button"
            onClick={() =>
              update(
                'Decode the Base64 signal Q1RGe3RydXN0X2J1dF92ZXJpZnl9 and return the plaintext.',
              )
            }
          >
            Try an example
          </button>
          {question && (
            <button type="button" className="text-button" onClick={() => update('')}>
              <X size={13} />
              Clear
            </button>
          )}
        </div>
        <div className="composer-bottom">
          <label className="title-field">
            <span className="visually-hidden">Challenge title</span>
            <input
              value={title}
              onChange={(event) => setTitle(event.target.value)}
              maxLength={120}
              placeholder="Operation name (optional)"
            />
          </label>
          <button
            className="primary solve-button"
            disabled={busy || !health || !health.model_available || !question.trim()}
            type="submit"
          >
            {busy ? <LoaderCircle className="spin" size={17} /> : <Sparkles size={17} />}Interpret &
            solve
            <ArrowRight size={17} />
          </button>
        </div>
        {!health?.model_available && (
          <div className="composer-offline">
            <span className="amber-dot" />
            <span>
              {health?.model?.state === 'loading'
                ? 'Gemma is loading. Your draft is saved locally.'
                : 'Connect Gemma to interpret pasted text. Structured examples work without it.'}
            </span>
            <button type="button" onClick={onConnect}>
              Connect model <ArrowRight size={13} />
            </button>
          </div>
        )}
        {error && (
          <div className="error-box" role="alert">
            {error}
          </div>
        )}
      </form>
      <div className="pipeline-strip">
        {stages.map((stage, index) => {
          const done =
            stage.id === 'interpret'
              ? !!run?.spec
              : stage.id === 'execute'
                ? !!run?.result
                : stage.id === 'verify'
                  ? !!run?.verification?.passed
                  : !!run?.solution?.question_crosschecked; // Completion requires each stage's actual output.
          const active =
            run?.status === 'running' &&
            (run.stage === stage.id || (stage.id === 'interpret' && run.stage === 'repair'));
          return (
            <div key={stage.id} className={active ? 'active' : done ? 'done' : ''}>
              <span className="pipeline-number">
                {done && !active ? <CheckCheck size={14} /> : `0${index + 1}`}
              </span>
              <span>
                <strong>{stage.title}</strong>
                <small>{stage.detail}</small>
              </span>
              {active && <LoaderCircle size={14} className="spin" />}
            </div>
          );
        })}
      </div>
    </section>
  );
}

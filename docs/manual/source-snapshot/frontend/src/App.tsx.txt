import { useCallback, useEffect, useRef, useState } from 'react';
import {
  Activity,
  ArrowDownToLine,
  ArrowRight,
  ArrowUpRight,
  BookOpen,
  Box,
  Braces,
  Check,
  ChevronDown,
  ChevronRight,
  CircleHelp,
  Clock3,
  Code2,
  Cpu,
  Crosshair,
  Database,
  FileCode2,
  Fingerprint,
  FolderOpen,
  GitBranch,
  Globe2,
  History,
  LayoutDashboard,
  LoaderCircle,
  LockKeyhole,
  Menu,
  Network,
  Plus,
  Radio,
  Search,
  Settings2,
  Shield,
  ShieldCheck,
  Square,
  Terminal,
  Workflow,
  X,
  Zap,
} from 'lucide-react';
import { api, downloadJson } from './api';
import { Badge, ChallengeModal, Graph, Result, VerificationPanel, statusLabel } from './components';
import { examples } from './examples';
import { ProblemComposer } from './ProblemComposer';
import { ModelConnection } from './ModelConnection';
import type { Example, Health, Run, Verification } from './types';

/** Sidebar destinations are real application views, not external navigation placeholders. */
const navigation = [
  { id: 'overview', label: 'Overview', icon: LayoutDashboard },
  { id: 'library', label: 'Challenge lab', icon: Crosshair },
  { id: 'history', label: 'Operations', icon: Workflow },
  { id: 'evidence', label: 'Evidence vault', icon: Database },
  { id: 'settings', label: 'System', icon: Settings2 },
];
/** Terminal status membership controls polling, stop buttons, and dashboard aggregates. */
const activeStates = new Set(['running', 'awaiting_approval', 'queued']);
/** Empty editor for new work; examples remain available from the library. */
const customChallenge: Example = {
  id: 'custom',
  title: '',
  description: '',
  category: 'Custom',
  tag: 'GEMMA',
};

/** Formats a true measured duration without fabricating values for unfinished runs. */
function duration(milliseconds?: number): string {
  return milliseconds === undefined
    ? '—'
    : milliseconds < 1000
      ? `${milliseconds}ms`
      : `${(milliseconds / 1000).toFixed(1)}s`;
}
/** Converts evidence timestamps into a compact local console timestamp. */
function time(value: string): string {
  return new Date(value).toLocaleTimeString([], { hour12: false });
}

/** Coordinates live engine data, challenge creation, and the cyberpunk operations console. */
export default function App() {
  const [page, setPage] = useState('overview'); // Current workspace view.
  const [health, setHealth] = useState<Health | null>(null); // Actual C++/model health.
  const [runs, setRuns] = useState<Run[]>([]); // Persistent operation summaries.
  const [run, setRun] = useState<Run | null>(null); // Selected detailed execution snapshot.
  const [selectedId, setSelectedId] = useState(''); // Stable run selection across polling refreshes.
  const [example, setExample] = useState(examples[0]); // Ready-to-run library input for an empty workspace.
  const [modal, setModal] = useState<Example | null>(null); // Open editor's initial fixture.
  const [busy, setBusy] = useState(false); // Prevents duplicate submissions.
  const [notice, setNotice] = useState(''); // Dismissible action feedback.
  const [connectionError, setConnectionError] = useState(''); // Visible real service connection failure.
  const [query, setQuery] = useState(''); // Global operation/library search string.
  const [filter, setFilter] = useState('all'); // History status filter.
  const [tab, setTab] = useState('trace'); // Selected operation detail tab.
  const [menuOpen, setMenuOpen] = useState(false); // Mobile navigation visibility.
  const [live, setLive] = useState(false); // WebSocket connection state, not engine activity.
  const [expandedEvidence, setExpandedEvidence] = useState<string | null>(null); // Evidence payload disclosure.
  const search = useRef<HTMLInputElement>(null); // Keyboard shortcut target.
  const previousStatus = useRef(''); // Detects real terminal transitions to refresh aggregate metrics.

  /** Refreshes persisted summaries and reports an unavailable service explicitly. */
  const refresh = useCallback(async () => {
    try {
      const data = await api<Run[]>('/challenges');
      setRuns(data);
      setConnectionError('');
    } catch (error) {
      setConnectionError(
        error instanceof Error ? error.message : 'Unable to reach the C++ engine.',
      );
    }
  }, []);

  useEffect(() => {
    let disposed = false; // Avoid state writes after unmount.
    /** Polls dependency health independently from run state updates. */
    async function checkHealth() {
      try {
        const data = await api<Health>('/health');
        if (!disposed) setHealth(data);
      } catch {
        if (!disposed) setHealth(null);
      }
    }
    void refresh();
    void checkHealth();
    const refreshTimer = window.setInterval(() => void refresh(), 4000); // Aggregates update even when no run is selected.
    const healthTimer = window.setInterval(() => void checkHealth(), 15000); // Slow health checks avoid taxing a local model.
    /** Supports the standard search shortcut without hijacking ordinary typing. */
    function shortcuts(event: KeyboardEvent) {
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k') {
        event.preventDefault();
        search.current?.focus();
      }
    }
    window.addEventListener('keydown', shortcuts);
    return () => {
      disposed = true;
      window.clearInterval(refreshTimer);
      window.clearInterval(healthTimer);
      window.removeEventListener('keydown', shortcuts);
    };
  }, [refresh]);

  useEffect(() => {
    if (!selectedId) {
      setRun(null);
      setLive(false);
      return;
    }
    let disposed = false; // Stops stale run responses replacing a newly selected run.
    let fallbackBusy = false; // At most one REST fallback request in flight.
    const controller = new AbortController(); // Cancels outstanding reads when selection changes.
    /** Accepts only the selected run's authoritative snapshot. */
    function receive(snapshot: Run) {
      if (disposed || snapshot.id !== selectedId) return;
      setRun(snapshot);
      if (snapshot.status !== previousStatus.current) {
        previousStatus.current = snapshot.status;
        void refresh();
      }
    }
    /** REST fallback works when WebSockets are temporarily unavailable. */
    async function poll() {
      if (fallbackBusy || disposed) return;
      fallbackBusy = true;
      try {
        receive(await api<Run>(`/agent/${selectedId}/state`, undefined, controller.signal));
      } catch (error) {
        if (!disposed)
          setConnectionError(error instanceof Error ? error.message : 'Live state unavailable.');
      } finally {
        fallbackBusy = false;
      }
    }
    void poll();
    const socket = new WebSocket(
      `${location.protocol === 'https:' ? 'wss:' : 'ws:'}//${location.host}/api/v1/live`,
    ); // Same-origin WebSocket through Vite or Crow.
    socket.onopen = () => {
      setLive(true);
      socket.send(JSON.stringify({ id: selectedId }));
    };
    socket.onmessage = (event) => {
      try {
        const snapshot = JSON.parse(event.data);
        if (snapshot.id) receive(snapshot);
      } catch {
        /* A malformed frame never becomes execution evidence. */
      }
    };
    socket.onclose = () => {
      if (!disposed) setLive(false);
    };
    socket.onerror = () => {
      if (!disposed) setLive(false);
    };
    const timer = window.setInterval(() => {
      if (socket.readyState === WebSocket.OPEN) socket.send(JSON.stringify({ id: selectedId }));
      else void poll();
    }, 800); // Bounded snapshot streaming with REST fallback.
    return () => {
      disposed = true;
      controller.abort();
      window.clearInterval(timer);
      socket.close();
    };
  }, [selectedId, refresh]);

  useEffect(() => {
    if (!notice) return;
    const timer = window.setTimeout(() => setNotice(''), 6500);
    return () => window.clearTimeout(timer);
  }, [notice]); // Feedback clears after sufficient reading time.

  /** Opens a different workspace view and closes compact-screen navigation. */
  function navigate(destination: string) {
    setPage(destination);
    setMenuOpen(false);
    setQuery('');
  }
  /** Selects an existing operation and opens its live details. */
  function selectRun(value: Run) {
    setSelectedId(value.id);
    setRun(null);
    setPage('overview');
    setTab('trace');
    setQuery('');
  }
  /** Submits validated data, starts the correct pipeline, and selects the resulting run. */
  async function launch(request: unknown) {
    setBusy(true);
    try {
      const created = await api<Run>('/challenges', request);
      const started = await api<Run>(created.mode === 'agent' ? '/agent/start' : '/solve', {
        id: created.id,
      });
      setSelectedId(started.id);
      setRun(started);
      setPage('overview');
      setTab('trace');
      setModal(null);
      await refresh();
    } finally {
      setBusy(false);
    }
  }
  /** Launches a library example through the same real API used by edited challenges. */
  async function launchExample() {
    try {
      await launch({
        title: example.title,
        question: example.description,
        mode: example.actions ? 'agent' : 'solver',
        approval: 'assisted',
        max_steps: 12,
        ...(example.spec ? { spec: example.spec } : { actions: example.actions }),
      });
    } catch (error) {
      setNotice(error instanceof Error ? error.message : 'Launch failed.');
    }
  }
  /** Requests cancellation without claiming a running network call stops instantaneously. */
  async function stop() {
    if (!run) return;
    try {
      await api('/agent/stop', { id: run.id });
      setNotice('Stop requested. The engine will halt at the next operation boundary.');
    } catch (error) {
      setNotice(String(error));
    }
  }
  /** Approves only the exact currently displayed action ID. */
  async function approve() {
    if (!run?.pending_action) return;
    try {
      await api('/agent/approve', { id: run.id, action_id: run.pending_action.id });
      setNotice('Action approved. Execution will continue.');
    } catch (error) {
      setNotice(String(error));
    }
  }
  /** Independently recomputes the stored verification without overwriting its original audit trail. */
  async function reverify() {
    if (!run) return;
    try {
      const result = await api<Verification>('/verify', { id: run.id });
      setNotice(
        result.passed
          ? `Re-verification passed: ${result.checks.length} checks.`
          : 'Re-verification rejected the stored result.',
      );
    } catch (error) {
      setNotice(String(error));
    }
  }
  /** Stable callback keeps the modal focus lifecycle from restarting during parent refreshes. */
  const closeModal = useCallback(() => setModal(null), []);

  const active = runs.filter((item) => activeStates.has(item.status)).length; // Live/queued run count.
  const completed = runs.filter((item) => !activeStates.has(item.status)); // Terminal runs for metrics.
  const checked = completed.filter((item) => item.verification?.passed).length; // Actual deterministic/provenance pass count.
  const rate = completed.length ? Math.round((checked / completed.length) * 100) : null; // No denominator means no displayed percentage.
  const timings = completed.filter((item) => item.duration_ms !== undefined); // Runs with real measured durations.
  const mean = timings.length
    ? timings.reduce((total, item) => total + (item.duration_ms || 0), 0) / timings.length
    : undefined; // Arithmetic mean, including failures honestly.
  const evidenceCount = runs.reduce((total, item) => total + (item.evidence_count || 0), 0); // Persisted evidence records.
  const visibleRuns = runs.filter(
    (item) =>
      `${item.title} ${item.id} ${item.mode} ${item.status}`
        .toLowerCase()
        .includes(query.toLowerCase()) &&
      (filter === 'all' ||
        (filter === 'active' && activeStates.has(item.status)) ||
        (filter === 'checked' && item.verification?.passed) ||
        (filter === 'failed' && ['failed', 'rejected'].includes(item.status))),
  ); // Shared search/filter predicate.
  const visibleExamples = examples.filter((item) =>
    `${item.title} ${item.category} ${item.tag}`.toLowerCase().includes(query.toLowerCase()),
  ); // Library search projection.
  const currentSpec = run?.spec || (!selectedId ? example.spec : undefined); // Never show example topology as a run's computed input.
  const evidence = run?.evidence || []; // Selected run's actual evidence trail.
  const currentTitle = run?.title || (selectedId ? 'Loading operation…' : example.title); // Honest loading/ready title.
  const date = new Intl.DateTimeFormat('en-GB', { day: '2-digit', month: 'short', year: 'numeric' })
    .format(new Date())
    .toUpperCase(); // Operator-local calendar label.

  return (
    <div className="app-shell">
      <aside className={`sidebar ${menuOpen ? 'open' : ''}`}>
        <a
          className="brand"
          href="#"
          onClick={(event) => {
            event.preventDefault();
            navigate('overview');
          }}
          aria-label="R0WD0GG overview"
        >
          <span className="brand-symbol">
            R<span>↗</span>
          </span>
          <div>
            R0WD0GG<small>CTF OPERATIONS FRAMEWORK</small>
          </div>
        </a>
        <div className="workspace-label">
          <span className="workspace-icon">
            <Terminal size={20} />
          </span>
          <div>
            Local workspace<small>PERSONAL / OFFLINE FIRST</small>
          </div>
          <LockKeyhole size={16} />
        </div>
        <span className="nav-heading">WORKSPACE</span>
        <nav aria-label="Main navigation">
          {navigation.map((item) => (
            <button
              key={item.id}
              className={page === item.id ? 'nav-item active' : 'nav-item'}
              onClick={() => navigate(item.id)}
            >
              <item.icon size={22} />
              <span>{item.label}</span>
              {item.id === 'history' && active > 0 && <b>{active}</b>}
              {page === item.id && <span className="nav-active-dot" />}
            </button>
          ))}
        </nav>
        <div className="sidebar-divider" />
        <span className="nav-heading">ENGINE STACK</span>
        <div className="stack-item">
          <Code2 size={20} />
          <span>C++20 core</span>
          <i className={health ? 'online-dot' : 'offline-dot'} />
        </div>
        <div className="stack-item">
          <Cpu size={20} />
          <span>Gemma / llama.cpp</span>
          <i className={health?.model_available ? 'online-dot' : 'amber-dot'} />
        </div>
        <div className="stack-item">
          <ShieldCheck size={20} />
          <span>Policy enforcement</span>
          <i className={health ? 'online-dot' : 'offline-dot'} />
        </div>
        <div className="local-card">
          <Fingerprint size={25} />
          <strong>
            Your machine.
            <br />
            Your intelligence.
          </strong>
          <p>Local inference. Controlled execution. Verifiable results.</p>
          <span>
            <span className="tiny-dot" /> NO CLOUD DEPENDENCY
          </span>
        </div>
        <div className="sidebar-bottom">
          <span className="avatar">OS</span>
          <div>
            Operator<small>LOCAL ADMINISTRATOR</small>
          </div>
          <button
            className="icon-button"
            onClick={() => navigate('settings')}
            aria-label="Open system details"
          >
            <Settings2 size={20} />
          </button>
        </div>
      </aside>
      <div className="main-shell">
        <header className="topbar">
          <div className="breadcrumb">
            <button
              className="icon-button mobile-menu"
              onClick={() => setMenuOpen(!menuOpen)}
              aria-label="Toggle navigation"
            >
              <Menu size={24} />
            </button>
            <span>Workspace</span>
            <ChevronRight size={17} />
            <strong>{navigation.find((item) => item.id === page)?.label}</strong>
          </div>
          <div className="topbar-right">
            <div className="global-search">
              <Search size={19} />
              <input
                ref={search}
                value={query}
                onChange={(event) => {
                  setQuery(event.target.value);
                  if (page !== 'library') setPage('history');
                }}
                placeholder="Search operations…"
                aria-label="Search operations"
              />
              <kbd>⌘ K</kbd>
            </div>
            <div className="system-status">
              <span className={health ? 'online-dot' : 'offline-dot'} />
              {health ? 'SYSTEM ONLINE' : 'ENGINE OFFLINE'}
            </div>
            <span className="top-avatar">OS</span>
          </div>
        </header>
        <main>
          <div className="page-heading">
            <div>
              <span className="eyebrow">
                R0WD0GG / {page === 'overview' ? 'MISSION CONTROL' : page.toUpperCase()}
              </span>
              <h1>
                {page === 'overview'
                  ? 'Operator console'
                  : page === 'library'
                    ? 'Challenge lab'
                    : page === 'history'
                      ? 'Operation history'
                      : page === 'evidence'
                        ? 'Evidence vault'
                        : 'System architecture'}
                <span className="heading-dot">.</span>
              </h1>
              <p>
                {page === 'overview'
                  ? 'LOCAL HOST // CTF WORKBENCH // EVIDENCE FIRST'
                  : page === 'library'
                    ? 'Pick a starting point. Every example runs through the real C++ engine.'
                    : page === 'history'
                      ? 'Every operation, every outcome. Persisted on your machine.'
                      : page === 'evidence'
                        ? 'Trace each conclusion back to an actual observation.'
                        : 'The model proposes. The deterministic engine decides.'}
              </p>
            </div>
            <button className="primary" onClick={() => setModal(customChallenge)}>
              <Plus size={20} />
              New operation
            </button>
          </div>
          {connectionError && (
            <div className="connection-banner" role="alert">
              <Radio size={22} />
              <div>
                <strong>Engine connection unavailable</strong>
                <p>{connectionError}</p>
              </div>
              <button className="secondary" onClick={() => void refresh()}>
                Retry
              </button>
            </div>
          )}
          {page === 'overview' && (
            <div className="operator-layout">
              <ProblemComposer
                health={health}
                busy={busy}
                run={run}
                onLaunch={launch}
                onConnect={() => navigate('settings')}
              />
              <div className="stats-grid">
                <div className="stat-card">
                  <span>
                    ACTIVE OPERATIONS
                    <Activity size={20} />
                  </span>
                  <strong>
                    {String(active).padStart(2, '0')}
                    <small className="muted">/ {runs.length} TOTAL</small>
                  </strong>
                  <p>
                    <span className="tiny-dot" />
                    {active ? 'Execution in progress' : 'Ready for your next challenge'}
                  </p>
                </div>
                <div className="stat-card">
                  <span>
                    CHECK PASS RATE
                    <ShieldCheck size={20} />
                  </span>
                  <strong>
                    {rate === null ? '—' : `${rate}%`}
                    <small className="stat-tag">DETERMINISTIC</small>
                  </strong>
                  <p>
                    {checked} passed / {completed.length} completed operations
                  </p>
                </div>
                <div className="stat-card">
                  <span>
                    AVG. EXECUTION TIME
                    <Clock3 size={20} />
                  </span>
                  <strong>
                    {duration(mean)}
                    <small className="muted">WALL CLOCK</small>
                  </strong>
                  <p>Measured across completed operations</p>
                </div>
                <div className="stat-card">
                  <span>
                    EVIDENCE COLLECTED
                    <Database size={20} />
                  </span>
                  <strong>
                    {String(evidenceCount).padStart(2, '0')}
                    <small className="muted">RECORDS</small>
                  </strong>
                  <p>
                    <LockKeyhole size={15} />
                    Persisted locally for inspection
                  </p>
                </div>
              </div>
              <div className="section-label">
                <h2>
                  <span className="tiny-dot" />
                  Execution workspace
                </h2>
                <span className="micro">
                  {date} <span className="muted">/</span> LOCAL SESSION
                </span>
              </div>
              <div className="execution-grid">
                <section className="panel operation-panel">
                  <div className="operation-header">
                    <div className="operation-icon">
                      <Network size={26} />
                    </div>
                    <div>
                      <span className="micro">
                        {run
                          ? `OP-${run.id.slice(0, 6).toUpperCase()} / ${run.mode === 'solver' ? 'MODE A' : 'MODE B'}`
                          : 'EXAMPLE INPUT / READY TO EXECUTE'}
                      </span>
                      <h2>{currentTitle}</h2>
                    </div>
                    {run ? (
                      <Badge status={run.status} />
                    ) : (
                      <span className="badge ready">
                        <span />
                        Ready
                      </span>
                    )}
                  </div>
                  <div className="operation-description">
                    <p>
                      {run?.question ||
                        (!selectedId
                          ? example.description
                          : 'Retrieving the saved state from the C++ engine.')}
                    </p>
                    <div className="operation-chips">
                      <span>
                        <GitBranch size={16} />
                        {run?.spec?.problem.type ||
                          (!selectedId ? example.tag : run?.mode || 'Loading')}
                      </span>
                      <span>
                        <Shield size={16} />
                        Policy enforced
                      </span>
                      <span>
                        <Cpu size={16} />
                        {run?.mode === 'agent'
                          ? `${run.steps}/${run.max_steps} steps`
                          : 'C++20 engine'}
                      </span>
                    </div>
                  </div>
                  {!selectedId && (
                    <div className="ready-controls">
                      <label htmlFor="ready-example">CHALLENGE</label>
                      <select
                        id="ready-example"
                        value={example.id}
                        onChange={(event) =>
                          setExample(examples.find((item) => item.id === event.target.value)!)
                        }
                      >
                        {examples.map((item) => (
                          <option value={item.id} key={item.id}>
                            {item.title}
                          </option>
                        ))}
                      </select>
                      <button
                        className="primary compact"
                        disabled={busy || !health}
                        onClick={() => void launchExample()}
                      >
                        {busy ? <LoaderCircle size={18} className="spin" /> : <Zap size={18} />}Run
                        challenge
                        <ArrowUpRight size={18} />
                      </button>
                      <button
                        className="icon-button"
                        aria-label="Edit challenge"
                        onClick={() => setModal(example)}
                      >
                        <Settings2 size={20} />
                      </button>
                    </div>
                  )}
                  <div className="detail-tabs" role="tablist" aria-label="Execution details">
                    {[
                      { id: 'trace', label: 'Live trace', icon: Terminal },
                      { id: 'input', label: 'Structured input', icon: Braces },
                      { id: 'evidence', label: 'Evidence', icon: Database },
                    ].map((item) => (
                      <button
                        role="tab"
                        aria-selected={tab === item.id}
                        key={item.id}
                        className={tab === item.id ? 'selected' : ''}
                        onClick={() => setTab(item.id)}
                      >
                        <item.icon size={18} />
                        {item.label}
                        {item.id === 'evidence' && <span>{evidence.length}</span>}
                      </button>
                    ))}
                    <span className="stream-label">
                      <span className={live ? 'online-dot' : 'offline-dot'} />
                      {live ? 'LIVE' : selectedId ? 'REST' : 'STANDBY'}
                    </span>
                  </div>
                  {tab === 'trace' && (
                    <>
                      {currentSpec?.category === 'graph' && (
                        <Graph spec={currentSpec} path={(run?.result?.path || []) as number[]} />
                      )}
                      <div className="trace-console">
                        <div className="console-heading">
                          <span>
                            <Terminal size={17} />
                            EXECUTION LOG
                          </span>
                          <span>{run ? `${evidence.length} EVENTS` : 'WAITING FOR INPUT'}</span>
                        </div>
                        {evidence.length ? (
                          evidence.map((item, index) => (
                            <div
                              className={`trace-line ${item.source === 'failed' ? 'error' : ''}`}
                              key={item.id}
                            >
                              <span className="trace-time">{time(item.timestamp)}</span>
                              <span className="trace-stage">{item.source.toUpperCase()}</span>
                              <span>{item.message}</span>
                              {index === evidence.length - 1 &&
                                activeStates.has(run?.status || '') && <span className="cursor" />}
                            </div>
                          ))
                        ) : (
                          <>
                            <div className="trace-line">
                              <span className="trace-time">SYSTEM</span>
                              <span className="trace-stage">READY</span>
                              <span>Challenge loaded. Awaiting execution.</span>
                            </div>
                            <div className="trace-line muted">
                              <span className="trace-time">ENGINE</span>
                              <span className="trace-stage">INFO</span>
                              <span>
                                The execution trace will appear here.
                                <span className="cursor" />
                              </span>
                            </div>
                          </>
                        )}
                      </div>
                    </>
                  )}
                  {tab === 'input' && (
                    <div className="json-view">
                      <div className="micro">
                        {run?.spec || example.spec ? 'CTF-IR / VERSION 1.0' : 'ACTION PLAN'}
                      </div>
                      <pre>
                        {JSON.stringify(
                          run?.spec ||
                            (!selectedId
                              ? example.spec || example.actions
                              : { status: 'Awaiting structured interpretation' }),
                          null,
                          2,
                        )}
                      </pre>
                    </div>
                  )}
                  {tab === 'evidence' && (
                    <div className="evidence-list">
                      {evidence.length ? (
                        evidence.map((item) => (
                          <div className="evidence-item" key={item.id}>
                            <button
                              onClick={() =>
                                setExpandedEvidence(expandedEvidence === item.id ? null : item.id)
                              }
                              aria-expanded={expandedEvidence === item.id}
                            >
                              <span className="micro">{item.id}</span>
                              <span>{item.message}</span>
                              <ChevronDown size={19} />
                            </button>
                            {expandedEvidence === item.id && (
                              <pre>{JSON.stringify(item.content, null, 2)}</pre>
                            )}
                          </div>
                        ))
                      ) : (
                        <div className="empty-inline">
                          <Database size={28} />
                          <p>No observations yet. Run the challenge to collect evidence.</p>
                        </div>
                      )}
                    </div>
                  )}
                  {run?.pending_action && (
                    <div className="approval-box">
                      <span className="eyebrow">OPERATOR APPROVAL REQUIRED</span>
                      <h3>{run.pending_action.action}</h3>
                      <p>
                        {run.pending_action.reason ||
                          'Review the proposed action before execution.'}
                      </p>
                      <pre>{JSON.stringify(run.pending_action.arguments, null, 2)}</pre>
                      <button className="primary compact" onClick={() => void approve()}>
                        <Check size={18} />
                        Approve this action
                      </button>
                      <button className="text-button" onClick={() => void stop()}>
                        Reject & stop
                      </button>
                    </div>
                  )}
                  {run?.error && (
                    <div className="error-box" role="alert">
                      {run.error}
                    </div>
                  )}
                  {run?.result && <Result run={run} notify={setNotice} />}
                  {run &&
                    ['needs_input', 'needs_review', 'failed', 'computed'].includes(run.status) && (
                      <div className="clarification-box">
                        <h3>
                          {run.status === 'needs_input'
                            ? 'More information needed'
                            : 'Refine and retry'}
                        </h3>
                        {run.missing_information?.map((question, index) => (
                          <p key={index}>{question}</p>
                        ))}
                        <button
                          className="secondary"
                          onClick={() =>
                            setModal({
                              ...customChallenge,
                              title: run.title,
                              description: run.request?.question || run.question || '',
                              ...(run.request?.spec ? { spec: run.request.spec } : {}),
                              ...(run.request?.actions ? { actions: run.request.actions } : {}),
                            })
                          }
                        >
                          Edit challenge & retry <ArrowRight size={18} />
                        </button>
                      </div>
                    )}
                  <div className="operation-footer">
                    <span>
                      <LockKeyhole size={16} />
                      {run
                        ? `Duration ${duration(run.duration_ms)}`
                        : 'All computation stays on your machine'}
                    </span>
                    {run && activeStates.has(run.status) ? (
                      <button className="danger-button" onClick={() => void stop()}>
                        <Square size={16} />
                        Stop execution
                      </button>
                    ) : (
                      <button
                        className="text-button"
                        onClick={() => {
                          setSelectedId('');
                          setRun(null);
                          setModal(example);
                        }}
                      >
                        Configure challenge <ArrowRight size={18} />
                      </button>
                    )}
                  </div>
                </section>
                <VerificationPanel run={run} onVerify={() => void reverify()} />
              </div>
              <div className="section-label recent-title">
                <h2>
                  Recent operations <span className="count-pill">{runs.length}</span>
                </h2>
                <button className="text-button" onClick={() => navigate('history')}>
                  View all operations <ArrowRight size={18} />
                </button>
              </div>
              <RunTable runs={runs.slice(0, 4)} onSelect={selectRun} />
            </div>
          )}
          {page === 'library' && (
            <>
              <div className="library-banner">
                <Crosshair size={30} />
                <div>
                  <h2>Start with a known challenge.</h2>
                  <p>Inspect the input, adapt the parameters, and watch the proof take shape.</p>
                </div>
                <span className="micro">{health?.capabilities.length || 21} SOLVERS / 2 MODES</span>
              </div>
              <div className="library-grid">
                {visibleExamples.map((item, index) => (
                  <article className="library-card" key={item.id}>
                    <div className="library-card-top">
                      <span className="library-number">0{index + 1}</span>
                      <span className="micro">{item.tag}</span>
                    </div>
                    <span className="eyebrow">{item.category}</span>
                    <h2>{item.title}</h2>
                    <p>{item.description}</p>
                    <button
                      className="secondary"
                      onClick={() => {
                        setExample(item);
                        setModal(item);
                      }}
                    >
                      Open challenge <ArrowUpRight size={20} />
                    </button>
                  </article>
                ))}
              </div>
              {!visibleExamples.length && (
                <div className="empty-state">
                  <Search size={30} />
                  <h3>No matching examples</h3>
                  <p>Try graph, CIDR, crypto, or agent.</p>
                </div>
              )}
            </>
          )}
          {page === 'history' && (
            <>
              <div className="history-toolbar">
                <div className="segmented">
                  {['all', 'active', 'checked', 'failed'].map((value) => (
                    <button
                      key={value}
                      className={filter === value ? 'selected' : ''}
                      onClick={() => setFilter(value)}
                    >
                      {value === 'all'
                        ? 'All operations'
                        : value === 'checked'
                          ? 'Checks passed'
                          : value}
                    </button>
                  ))}
                </div>
                <span className="micro">{visibleRuns.length} OPERATIONS</span>
                <button
                  className="secondary"
                  onClick={() => downloadJson('rowdogg-operations.json', visibleRuns)}
                  disabled={!visibleRuns.length}
                >
                  <ArrowDownToLine size={18} />
                  Export summaries
                </button>
              </div>
              <RunTable runs={visibleRuns} onSelect={selectRun} />
            </>
          )}
          {page === 'evidence' && (
            <section className="panel vault">
              <div className="panel-heading">
                <h2>
                  <Database size={22} />
                  Recorded observations
                </h2>
                <select
                  aria-label="Choose operation evidence"
                  value={selectedId}
                  onChange={(event) => setSelectedId(event.target.value)}
                >
                  <option value="">Select an operation</option>
                  {runs.map((item) => (
                    <option key={item.id} value={item.id}>
                      {item.title} / {item.id.slice(0, 6)}
                    </option>
                  ))}
                </select>
                <button
                  className="secondary"
                  disabled={!run?.evidence?.length}
                  onClick={() =>
                    run && downloadJson(`evidence-${run.id.slice(0, 8)}.json`, run.evidence)
                  }
                >
                  <ArrowDownToLine size={18} />
                  Export
                </button>
              </div>
              {evidence.length ? (
                evidence.map((item) => (
                  <article className="vault-record" key={item.id}>
                    <div>
                      <span className="evidence-id">{item.id}</span>
                      <span className="micro">{item.source.toUpperCase()}</span>
                      <time>{item.timestamp}</time>
                    </div>
                    <h3>{item.message}</h3>
                    <details>
                      <summary>
                        Inspect raw payload <ChevronDown size={18} />
                      </summary>
                      <pre>{JSON.stringify(item.content, null, 2)}</pre>
                    </details>
                  </article>
                ))
              ) : (
                <div className="empty-state">
                  <Fingerprint size={38} />
                  <h3>Follow the evidence.</h3>
                  <p>
                    Select an operation to inspect its timestamped observations and verification
                    records.
                  </p>
                </div>
              )}
            </section>
          )}
          {page === 'settings' && (
            <>
              <ModelConnection
                onUpdate={() => {
                  void api<Health>('/health')
                    .then(setHealth)
                    .catch(() => setHealth(null));
                }}
              />
              <System health={health} />
            </>
          )}
          <footer className="main-footer">
            <span>
              <span className="tiny-dot" />
              R0WD0GG FRAMEWORK <span className="muted">/</span> V0.1.0
            </span>
            <span>
              LOCAL BY DESIGN. VERIFIED BY COMPUTATION.
              <ShieldCheck size={17} />
            </span>
          </footer>
        </main>
      </div>
      {modal && (
        <ChallengeModal initial={modal} onClose={closeModal} onSubmit={launch} busy={busy} />
      )}
      {notice && (
        <div className="toast" role="status">
          <Terminal size={22} />
          <span>{notice}</span>
          <button
            className="icon-button"
            onClick={() => setNotice('')}
            aria-label="Dismiss notification"
          >
            <X size={20} />
          </button>
        </div>
      )}
    </div>
  );
}

/** Shared history table is keyboard-accessible and gracefully handles empty results. */
function RunTable({ runs, onSelect }: { runs: Run[]; onSelect: (run: Run) => void }) {
  return (
    <div className="panel table-panel">
      {runs.length ? (
        <div className="table-scroll">
          <table>
            <thead>
              <tr>
                <th>OPERATION</th>
                <th>MODE</th>
                <th>STATUS</th>
                <th>DURATION</th>
                <th>EVIDENCE</th>
                <th>
                  <span className="visually-hidden">Open</span>
                </th>
              </tr>
            </thead>
            <tbody>
              {runs.map((run) => (
                <tr key={run.id}>
                  <td>
                    <button className="table-title" onClick={() => onSelect(run)}>
                      <span className="table-icon">
                        {run.mode === 'agent' ? <Workflow size={20} /> : <FileCode2 size={20} />}
                      </span>
                      <span>
                        {run.title}
                        <small>OP-{run.id.slice(0, 6).toUpperCase()}</small>
                      </span>
                    </button>
                  </td>
                  <td>
                    <span className="table-mode">
                      {run.mode === 'agent' ? 'AGENT / B' : 'SOLVER / A'}
                    </span>
                  </td>
                  <td>
                    <Badge status={run.status} />
                  </td>
                  <td className="mono">{duration(run.duration_ms)}</td>
                  <td className="mono">
                    {run.evidence_count ?? run.evidence?.length ?? 0} records
                  </td>
                  <td>
                    <button
                      className="icon-button"
                      aria-label={`Open ${run.title}`}
                      onClick={() => onSelect(run)}
                    >
                      <ArrowUpRight size={20} />
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      ) : (
        <div className="empty-history">
          <FolderOpen size={25} />
          <div>
            <strong>No operations here yet.</strong>
            <p>Launch a challenge to start building your execution history.</p>
          </div>
          <span className="micro">AWAITING FIRST RUN</span>
        </div>
      )}
    </div>
  );
}

/** Documents the running architecture and immutable safety boundaries without fake settings. */
function System({ health }: { health: Health | null }) {
  return (
    <div className="system-grid">
      <section className="panel">
        <div className="panel-heading">
          <h2>
            <Box size={22} />
            Service topology
          </h2>
          <span className="micro">LOCAL MACHINE</span>
        </div>
        <div className="architecture-flow">
          {[
            {
              icon: Globe2,
              title: 'React operations console',
              detail: 'TypeScript + Vite / localhost:3000',
              status: 'UI active',
            },
            {
              icon: Cpu,
              title: 'Crow / C++20 engine',
              detail: 'REST + WebSocket / localhost:8080',
              status: health ? 'Connected' : 'Offline',
            },
            {
              icon: Braces,
              title: 'CTF-IR validation boundary',
              detail: 'Version 1.0 / strict type and resource checks',
              status: 'Enforced in C++',
            },
            {
              icon: ShieldCheck,
              title: 'Solve → verify → review',
              detail: 'Deterministic engine + independent checks',
              status: 'No model execution',
            },
          ].map((item) => (
            <div className="architecture-node" key={item.title}>
              <span>
                <item.icon size={26} />
              </span>
              <div>
                <h3>{item.title}</h3>
                <p>{item.detail}</p>
              </div>
              <small>{item.status}</small>
            </div>
          ))}
        </div>
      </section>
      <section className="panel system-details">
        <div className="panel-heading">
          <h2>
            <LockKeyhole size={22} />
            Execution boundaries
          </h2>
        </div>
        <dl>
          <dt>Model endpoint</dt>
          <dd>
            127.0.0.1:8081{' '}
            <span className={health?.model_available ? 'pass' : 'amber-text'}>
              {health?.model_available ? 'ONLINE' : 'OFFLINE'}
            </span>
          </dd>
          <dt>Authorized lab origin</dt>
          <dd>{health?.lab_origin || 'http://127.0.0.1:8090'}</dd>
          <dt>Tool budget</dt>
          <dd>1–32 actions per run</dd>
          <dt>Request timeout</dt>
          <dd>Lab: 5s / Model: 60s</dd>
          <dt>Observation size</dt>
          <dd>64 KiB maximum</dd>
          <dt>Execution policy</dt>
          <dd>Named tools only. No shell execution.</dd>
          <dt>Persistence</dt>
          <dd>Local JSON snapshots / data/runs</dd>
        </dl>
      </section>
      <section className="panel capability-panel">
        <div className="panel-heading">
          <h2>
            <Code2 size={22} />
            Registered deterministic solvers
          </h2>
          <span className="micro">
            {health ? `${health.capabilities.length} AVAILABLE` : 'ENGINE OFFLINE'}
          </span>
        </div>
        <div className="capabilities">
          {health?.capabilities.map((capability) => (
            <span key={capability}>
              <Check size={16} />
              {capability.replaceAll('_', ' ')}
            </span>
          )) || <p className="muted">Start the backend to inspect its actual solver registry.</p>}
        </div>
      </section>
      <section className="panel getting-started">
        <div className="panel-heading">
          <h2>
            <BookOpen size={22} />
            Local model setup
          </h2>
        </div>
        <p>
          Structured challenges and supplied action plans run without a language model. To enable
          natural-language interpretation and planning, serve a compatible Gemma GGUF locally:
        </p>
        <pre>
          llama-server -m /path/to/gemma.gguf
          <br /> --host 127.0.0.1 --port 8081 -c 8192
        </pre>
        <p className="muted">
          The UI reports actual model health. Model weights are not bundled. See README.md for
          build, lab, test, and deployment instructions.
        </p>
      </section>
    </div>
  );
}

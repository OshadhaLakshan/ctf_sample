/** JSON objects are kept open at the versioned CTF-IR boundary. */
export type JsonObject = Record<string, unknown>;

/** A deterministic check has a stable machine name and an explicit verdict. */
export interface Check {
  name: string;
  passed: boolean;
}
/** Verification is separate from the model's semantic opinion. */
export interface Verification {
  passed: boolean;
  checks: Check[];
  method: string;
}
/** Evidence is an immutable, timestamped observation from the C++ engine. */
export interface Evidence {
  id: string;
  timestamp: string;
  source: string;
  message: string;
  content: JsonObject;
}
/** A tool proposal is data until policy validation and any approval succeed. */
export interface Action {
  id?: string;
  action: string;
  arguments: JsonObject;
  reason?: string;
}
/** UI-facing CTF-IR shape; C++ performs authoritative validation. */
export interface Spec {
  version: string;
  category: string;
  problem: { type: string };
  input: JsonObject;
  parameters: JsonObject;
  output: { type: string };
}
/** Durable execution snapshot returned by both REST and WebSocket. */
export interface Run {
  id: string; // Server-generated immutable run identifier.
  title: string; // Operator-facing challenge title.
  question: string; // Untrusted natural-language statement.
  mode: 'solver' | 'agent'; // Deterministic or multi-step execution path.
  approval: string; // Manual, assisted, or autonomous lab policy.
  status: string; // Actual engine state; never inferred from animations.
  stage: string; // Most recent pipeline stage.
  created_at: string; // UTC creation time.
  updated_at: string; // UTC latest persisted transition.
  steps: number; // Number of executed tool attempts.
  max_steps: number; // Hard action budget.
  duration_ms?: number; // Monotonic elapsed duration when terminal.
  evidence?: Evidence[]; // Full observations only in detailed snapshots.
  evidence_count?: number; // Compact count for dashboard summaries.
  result: JsonObject | null; // Deterministic result or latest agent observation.
  verification: Verification | null; // Explicit computational/provenance verdict.
  semantic_review: {
    status: string;
    answers_question?: boolean;
    confidence?: number;
    reason?: string;
  }; // Separate model assessment.
  spec?: Spec; // Accepted CTF-IR used by the solver.
  pending_action?: Action; // Exact proposal awaiting one-shot approval.
  error?: string; // Actionable server error without invented recovery.
}
/** Real dependency health, queried from the C++ service. */
export interface Health {
  status: string;
  model_available: boolean;
  lab_origin: string;
  capabilities: string[];
}
/** Library entries are editable example inputs, never simulated execution results. */
export interface Example {
  id: string;
  title: string;
  category: string;
  description: string;
  tag: string;
  spec?: Spec;
  actions?: Action[];
}

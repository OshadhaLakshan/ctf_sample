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
  missing_information?: string[]; // Specific clarification questions returned by the interpreter.
  solution?: { answer: JsonObject; explanation: string; question_crosschecked: boolean }; // Final explanation grounded in the engine result.
  request?: { title?: string; question?: string; spec?: Spec; actions?: Action[]; mode?: string }; // Original input for an editable retry.
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
    input_matches_question?: boolean;
    explanation?: string;
  }; // Separate model assessment.
  spec?: Spec; // Accepted CTF-IR used by the solver.
  pending_action?: Action; // Exact proposal awaiting one-shot approval.
  error?: string; // Actionable server error without invented recovery.
}
/** Real dependency health, queried from the C++ service. */
export interface Health {
  model?: ModelStatus; // Detailed local inference diagnostics, separate from API health.
  status: string;
  model_available: boolean;
  lab_origin: string;
  capabilities: string[];
}
/** Non-secret, operator-controlled llama.cpp connection configuration. */
export interface ModelConfig {
  base_url: string; // Loopback server root, normalized by C++.
  model: string; // Exact discovered model ID; empty selects the server's first model.
  timeout_seconds: number; // Total inference deadline, including long CPU generation.
  max_tokens: number; // Bound for structured output generation.
}
/** Connection readiness does not imply a successful generation test. */
export interface ModelStatus {
  available: boolean; // Whether the service is ready to accept inference requests.
  state: string; // ready, offline, loading, unauthorized, wrong_endpoint, or model_missing.
  message: string; // Actionable diagnostic from the actual transport response.
  config: ModelConfig; // Persisted public configuration.
  models: string[]; // Exact names returned by /v1/models.
  selected_model: string; // Effective auto-discovered or explicit model ID.
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

# Repo Task: Curate & Reorganize `docs/` + Add Frontend Pages Guide + Update Root README

## Objective

Clean up and reorganize the `docs/` folder by deleting redundant/irrelevant Markdown files, consolidating overlaps, and producing a clear, minimal documentation set that’s useful for new users and contributors. Then, update the repository `README.md` with a “Docs Map” that links to the reorganized docs. Finally, create a dedicated markdown describing each frontend page (with image placeholders under `docs/images/`).

## Current Inputs

Project root contains a `docs/` directory with these files (1 dir, 37 files):

```
BACKEND_INTEGRATION_STATUS.md
BACKEND_LOGS_PAGE_COMPLETE.md
BACKEND_MONITORING_COMPLETE.md
BACKEND_MONITORING_SETUP.md
COMPLIANCE_REPORT.md
COMTRADE_PARSER.md
CROSS_PLATFORM_IMPLEMENTATION.md
CROSS_PLATFORM_QUICK_REFERENCE.md
CROSS_PLATFORM.md
cross-plataform.md
DASHBOARD_BACKEND_INTEGRATION.md
DOCKER_MACOS_GUIDE.md
E2E_IMPLEMENTATION_SUMMARY.md
E2E_VERIFICATION.md
FRONTEND_BACKEND_INTEGRATION_COMPLETE.md
FRONTEND_COMPLETION.md
GETTING_STARTED.md
IMPLEMENTATION_COMPLETE.md
IMPLEMENTATION_FINAL_SUMMARY.md
IMPLEMENTATION_PROGRESS.md
IMPLEMENTATION_SUMMARY.md
INTEGRATION_VERIFICATION.md
MACOS_BPF_WEBSOCKET.md
MACOS_NETWORK_SETUP.md
PHASE1_COMPLETION.md
README-MACOS-NETWORKING.md
ROADMAP.md
TASK_10_ANALYZER.md
TASK_13_DESIGN_SYSTEM.md
TASK_15_STATE_ROUTING_API.md
TASK_16_STREAM_MANAGEMENT.md
TASK_17_MANUAL_INJECTION.md
TASK_6_COMPLETION.md
TASK_8_COMPLETION.md
TESTS_QUICK_START.md
UNIT_TESTS_IMPLEMENTATION.md
UNIT_TESTS_SUMMARY.md
```

## Deliverables

1. **Reorganized `docs/` structure** (create folders; move/rename files):

   ```
   docs/
   ├── 00-overview/
   │   └── getting-started.md
   ├── 01-architecture/
   │   ├── system-overview.md
   │   ├── backend-architecture.md
   │   └── frontend-architecture.md
   ├── 02-setup/
   │   ├── docker-macos-guide.md
   │   ├── macos-network-setup.md
   │   └── macos-bpf-websocket.md
   ├── 03-backend/
   │   ├── monitoring-setup.md
   │   ├── logs-and-monitoring.md
   │   ├── integration-status.md
   │   └── comtrade-parser.md
   ├── 04-frontend/
   │   ├── backend-integration.md
   │   └── pages.md   ← (NEW: see template below)
   ├── 05-cross-platform/
   │   ├── overview.md
   │   └── quick-reference.md
   ├── 06-tests/
   │   ├── tests-quick-start.md
   │   ├── unit-tests.md
   │   └── e2e-verification.md
   ├── 07-roadmap/
   │   └── roadmap.md
   ├── 98-archive/        ← keep non-user-facing, historical logs/summaries
   └── images/            ← image placeholders live here
   ```

   * The **`98-archive/`** folder stores progress logs, “phase completion” notes, and verbose implementation summaries that aren’t needed by end users but may be useful historically.
   * Delete obvious duplicates and low-value docs per the rules below.

2. **New `docs/04-frontend/pages.md`** with per-page descriptions and image placeholders (see template).

3. **Root `README.md`** updated with a **Docs Map** section that links to each organized doc.

4. **A machine-readable manifest** `docs/docs-manifest.json` enumerating kept/renamed/deleted files and rationale (e.g., `{ "path": "...", "action": "kept|renamed|deleted|merged", "reason": "..." }`).

5. **A single PR** with:

   * Before/after `docs/` tree (as a code block in PR description).
   * Summary of deletions/merges.
   * Links proving cross-platform duplicates were merged.
   * Checklist of acceptance criteria (see below).

## Deletion & Consolidation Rules

Apply these rules **in order**:

1. **Exact duplicates** (identical hash) → **Delete** duplicates, keep the best-named/most current file.
2. **Near duplicates** (same topic; >70% overlapping headings or content):

   * **Merge** into one clear document with normalized naming.
   * Preserve the most complete content; add a short “Notes” section if needed.
3. **Completion/Progress logs** (`*_COMPLETION.md`, `IMPLEMENTATION_*`, `PHASE1_COMPLETION.md`, `IMPLEMENTATION_PROGRESS.md`, `FRONTEND_COMPLETION.md`, `BACKEND_*_COMPLETE.md`, `E2E_IMPLEMENTATION_SUMMARY.md`, `IMPLEMENTATION_FINAL_SUMMARY.md`, `IMPLEMENTATION_SUMMARY.md`):

   * Move to **`docs/98-archive/`** unless they contain unique, actionable guidance for users; in that case, **extract** guidance into relevant topical docs and archive the log.
4. **Cross-platform cluster** (`CROSS_PLATFORM.md`, `CROSS_PLATFORM_IMPLEMENTATION.md`, `CROSS_PLATFORM_QUICK_REFERENCE.md`, `cross-plataform.md`):

   * **Merge** into:

     * `docs/05-cross-platform/overview.md` (long-form guidance)
     * `docs/05-cross-platform/quick-reference.md` (short checklist/commands)
   * Fix spelling and keep kebab-case file names.
5. **Platform setup/networking** (`DOCKER_MACOS_GUIDE.md`, `MACOS_NETWORK_SETUP.md`, `README-MACOS-NETWORKING.md`, `MACOS_BPF_WEBSOCKET.md`):

   * Consolidate repetitive sections; keep:

     * `docs/02-setup/docker-macos-guide.md`
     * `docs/02-setup/macos-network-setup.md`
     * `docs/02-setup/macos-bpf-websocket.md`
6. **Backend monitoring/integration** (`BACKEND_MONITORING_SETUP.md`, `BACKEND_MONITORING_COMPLETE.md`, `BACKEND_LOGS_PAGE_COMPLETE.md`, `BACKEND_INTEGRATION_STATUS.md`):

   * Extract user-facing steps into:

     * `docs/03-backend/monitoring-setup.md`
     * `docs/03-backend/logs-and-monitoring.md`
     * `docs/03-backend/integration-status.md`
   * Archive remaining “complete” logs.
7. **Testing** (`TESTS_QUICK_START.md`, `UNIT_TESTS_IMPLEMENTATION.md`, `UNIT_TESTS_SUMMARY.md`, `E2E_VERIFICATION.md`):

   * Keep:

     * `docs/06-tests/tests-quick-start.md`
     * `docs/06-tests/unit-tests.md` (merge implementation + summary)
     * `docs/06-tests/e2e-verification.md`
8. **Frontend/backend integration** (`FRONTEND_BACKEND_INTEGRATION_COMPLETE.md`, `DASHBOARD_BACKEND_INTEGRATION.md`, `TASK_*` related to routing/stream mgmt):

   * Extract stable instructions into:

     * `docs/04-frontend/backend-integration.md`
     * References from `docs/01-architecture/frontend-architecture.md`
   * Archive task-specific logs (`TASK_*`) into `98-archive/` unless they contain reusable guides (then extract).
9. **Misc**:

   * `COMPLIANCE_REPORT.md` → keep only if actively useful; otherwise archive.
   * `ROADMAP.md` → move to `docs/07-roadmap/roadmap.md`.
   * `COMTRADE_PARSER.md` → keep under `docs/03-backend/comtrade-parser.md`.
   * `GETTING_STARTED.md` → move/rename to `docs/00-overview/getting-started.md`.
   * Create `docs/01-architecture/system-overview.md`, `backend-architecture.md`, `frontend-architecture.md` by extracting stable architecture info from existing files.

## Naming & Style Conventions

* **Filenames:** `kebab-case`, `.md`
* **Headings:** Start with a single `# Title`, then hierarchical `##`, `###`
* **Front-matter (optional):** none required
* **Linking:** Use **relative links** (e.g., `../02-setup/docker-macos-guide.md`)
* **Images:** Put all images in `docs/images/`; reference as `![Caption](../images/<name>.png)` from doc subfolders (use correct relative paths)
* **Commands:** Use fenced code blocks with language hints

## New File: `docs/04-frontend/pages.md` (Template)

Create and fill the skeleton with known routes/pages. Leave `TODO:` placeholders where details are unknown.

```markdown
# Frontend Pages Guide

> This guide describes each page in the frontend, its route, purpose, primary components/state, and a screenshot placeholder. Images should be placed under `docs/images/`.

## Page: Dashboard
- **Route:** `/dashboard`
- **Purpose:** High-level system status and recent activity.
- **Key Components:** `StatusCards`, `RecentActivity`, `AlertsPanel`
- **Primary State/Queries:** `useSystemStatus()`, `useAlerts()`
- **Image:** ![Dashboard](../images/dashboard.png) <!-- TODO: add final image -->

## Page: Streams
- **Route:** `/streams`
- **Purpose:** Manage and monitor streams.
- **Key Components:** `StreamTable`, `StreamConfigDialog`, `StreamMetrics`
- **Primary State/Queries:** `useStreams()`, `useStreamMetrics(id)`
- **Image:** ![Streams](../images/streams.png) <!-- TODO: add final image -->

## Page: Logs
- **Route:** `/logs`
- **Purpose:** Search and view backend logs.
- **Key Components:** `LogSearchBar`, `LogList`, `LogDetails`
- **Primary State/Queries:** `useLogs(query)`, `useLogDetails(id)`
- **Image:** ![Logs](../images/logs.png) <!-- TODO: add final image -->

## Page: Settings
- **Route:** `/settings`
- **Purpose:** App configuration (API keys, endpoints, feature flags).
- **Key Components:** `SettingsForm`, `FeatureToggles`
- **Primary State/Queries:** `useSettings()`, `useUpdateSettings()`
- **Image:** ![Settings](../images/settings.png) <!-- TODO: add final image -->

## Page: Monitoring
- **Route:** `/monitoring`
- **Purpose:** Metrics dashboards and health checks.
- **Key Components:** `MetricsCharts`, `HealthChecks`
- **Primary State/Queries:** `useMetrics()`, `useHealthChecks()`
- **Image:** ![Monitoring](../images/monitoring.png) <!-- TODO: add final image -->

## Page: Help
- **Route:** `/help`
- **Purpose:** Quick links to docs, troubleshooting steps, and FAQs.
- **Key Components:** `DocsLinks`, `Troubleshooting`, `FaqAccordion`
- **Image:** ![Help](../images/help.png) <!-- TODO: add final image -->

---
> Add more pages following the same structure.
```

## Update Root `README.md` (Docs Map Section)

Add a section like this (ensure paths are correct after reorg):

```markdown
## 📚 Docs Map

- **Overview**
  - [Getting Started](docs/00-overview/getting-started.md)
- **Architecture**
  - [System Overview](docs/01-architecture/system-overview.md)
  - [Backend Architecture](docs/01-architecture/backend-architecture.md)
  - [Frontend Architecture](docs/01-architecture/frontend-architecture.md)
- **Setup**
  - [Docker on macOS](docs/02-setup/docker-macos-guide.md)
  - [macOS Network Setup](docs/02-setup/macos-network-setup.md)
  - [macOS BPF/WebSocket Notes](docs/02-setup/macos-bpf-websocket.md)
- **Backend**
  - [Monitoring Setup](docs/03-backend/monitoring-setup.md)
  - [Logs & Monitoring](docs/03-backend/logs-and-monitoring.md)
  - [Integration Status](docs/03-backend/integration-status.md)
  - [COMTRADE Parser](docs/03-backend/comtrade-parser.md)
- **Frontend**
  - [Backend Integration](docs/04-frontend/backend-integration.md)
  - [Pages Guide (with images)](docs/04-frontend/pages.md)
- **Cross-Platform**
  - [Overview](docs/05-cross-platform/overview.md)
  - [Quick Reference](docs/05-cross-platform/quick-reference.md)
- **Testing**
  - [Tests Quick Start](docs/06-tests/tests-quick-start.md)
  - [Unit Tests](docs/06-tests/unit-tests.md)
  - [E2E Verification](docs/06-tests/e2e-verification.md)
- **Roadmap**
  - [Project Roadmap](docs/07-roadmap/roadmap.md)
- **Archive (historical)**
  - See `docs/98-archive/` for legacy progress logs and summaries.
```

## Implementation Steps

1. **Create branch:** `docs/reorg-<date>`
2. **Inventory + hashing:** Compute file hashes to find exact duplicates; detect near-dupes by title/heading overlap (≥70% shared headings).
3. **Apply rules:** Delete/merge/move/rename per rules above. Normalize to kebab-case.
4. **Extract stable content:** From “completion/progress” docs into topical guides; archive the rest.
5. **Create new files:**

   * `docs/04-frontend/pages.md` using the template.
   * `docs/01-architecture/*` if missing.
   * `docs/docs-manifest.json` with actions and reasons.
6. **Update root `README.md`:** Insert “Docs Map” section.
7. **Validate links:** Run a link checker (e.g., `markdown-link-check`) to ensure no broken relative links.
8. **Image placeholders:** Create `docs/images/.gitkeep`; ensure all page image links point to `docs/images/*.png`.
9. **Commit with clear messages:** One commit per major step, squash as needed.
10. **PR:** Include before/after tree and summary.

## Acceptance Criteria

* No broken links in Markdown (CI link check passes).
* Redundant/duplicate docs removed or merged; cross-platform docs consolidated.
* Root `README.md` contains an accurate “Docs Map”.
* `docs/04-frontend/pages.md` exists with routes, purposes, components, state, and image placeholders.
* `docs/docs-manifest.json` explains every deletion/merge/move.
* Final `docs/` tree matches the proposed structure (minor variations allowed if reasoned in manifest).
* All filenames are kebab-case; headings consistently structured.

## Notes & Constraints

* Prefer **fewer, higher-quality** docs over many status logs.
* Keep user-facing guidance concise; move verbose histories into `98-archive/`.
* Use relative links; avoid absolute URLs to repository paths.

---

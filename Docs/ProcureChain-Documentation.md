<h2 align="center">PROCURECHAIN</h2>
<h4 align="center">Municipal Procurement Document Tracking System</h4>

<br>

---

## PROPOSED TITLE

**A Proposed Terminal-Based Municipal Procurement Document Tracking System with Simulated Blockchain Verification and Role-Based Governance Using C++**

---

## ABSTRACT

This project presents the design of ProcureChain, a terminal-based municipal procurement document tracking system developed using procedural C++ programming. The system provides a structured workflow for uploading, reviewing, approving, and publishing procurement documents within a simulated local government setting. It employs role-based access control across five distinct user roles — Citizen, Super Admin, Procurement Officer, Budget Officer, and Municipal Administrator — each with defined permissions and responsibilities that mirror real-world governance hierarchies.

The core functionality encompasses full document lifecycle management, including multi-role consensus-based approval workflows, budget allocation and variance reporting, and delegated authority mechanisms. To ensure data integrity and transparency, the system integrates SHA-256 cryptographic hashing for password security and document verification, an append-only hash-linked audit trail, and a dynamic simulated blockchain topology that records all critical actions with cross-node consistency validation. Additional features include configurable approval rules with SLA thresholds, ranked full-text search with multi-criteria filtering, analytics dashboards for compliance monitoring and workload analysis, duplicate-hash upload safeguards, strict conflict-of-interest approval blocking, status-transition timeline logging, budget overrun publication guardrails, monthly transparency report generation (TXT/CSV), AI-assisted document summarization, public-audit document drill-down, and tamper-aware notification inboxes.

All data is persisted through structured, pipe-delimited text files, and the program is built entirely within the standard C++ library without external dependencies or object-oriented constructs. ProcureChain demonstrates how fundamental programming concepts — vectors, functions, file I/O, conditional logic, and modular design — can be applied to construct a governance-oriented system that addresses transparency, accountability, and procedural compliance in municipal procurement operations.

---

## BACKGROUND OF THE STUDY

Public procurement is a critical function of local government, involving the acquisition of goods, services, and infrastructure necessary for delivering public services. In the Philippines, government procurement is governed by Republic Act No. 9184, also known as the Government Procurement Reform Act, which mandates transparency, competitiveness, and accountability in all procurement transactions (Congress of the Philippines, 2003). Despite these legal frameworks, many local government units (LGUs) continue to rely on manual, paper-based processes for tracking procurement documents, resulting in inefficiencies, delays, and vulnerabilities to irregularities (Government Procurement Policy Board, 2023).

According to the Commission on Audit (COA, 2023), procurement-related findings consistently rank among the most common audit observations in local government operations. Issues such as missing documentation, incomplete approval chains, unauthorized modifications to procurement records, and lack of proper audit trails have been repeatedly identified as systemic weaknesses (Open Contracting Partnership, 2024). These problems are further compounded by the absence of accessible, low-cost digital tools that smaller municipalities can adopt without requiring extensive IT infrastructure or specialized technical personnel.

The challenge of maintaining document integrity throughout the procurement lifecycle is particularly significant. Once a procurement document is submitted, it must pass through multiple levels of review and approval before it can be considered valid and publishable. Any unauthorized alteration during this process can compromise the legitimacy of the entire transaction. Traditional document management approaches offer limited protection against tampering, as they typically lack mechanisms for verifying whether a document has been modified after submission (Bashir, 2020).

Furthermore, the lack of transparency in procurement processes erodes public trust in local governance. Citizens often have no practical means of verifying whether procurement documents are authentic or whether proper procedures were followed. This information asymmetry between government officials and the public undermines the principles of open governance and participatory democracy that Philippine law seeks to promote (Transparency International, 2024; Congress of the Philippines, 2003).

These challenges highlight the need for a system that can manage the complete procurement document lifecycle — from submission through multi-level approval to publication — while maintaining verifiable records of all actions taken. Such a system should enforce proper workflow procedures, provide tamper-evident audit trails, and enable public verification of document authenticity, all within a framework that is accessible to municipalities with limited technical resources (Philippine Government Electronic Procurement System, 2024).

---

## OBJECTIVES OF THE STUDY

This project aims to design a municipal procurement document tracking system using the C++ programming language that seeks to apply fundamental programming concepts covered during the finals period of the academic year 2025–2026 while implementing governance-oriented features for procurement transparency and accountability.

Specifically:

1. To design a functional procurement document tracking system that allows authorized personnel to upload, review, approve, and publish procurement documents through structured CRUD operations and a multi-role approval workflow.

2. To apply essential C++ programming concepts such as loops, conditional statements, functions, vectors, file I/O, and string manipulation in building a modular, procedural system capable of managing complex governance workflows.

3. To implement role-based access control across five user roles — Citizen, Super Admin, Procurement Officer, Budget Officer, and Municipal Administrator — ensuring that each role has defined permissions and responsibilities aligned with proper procurement governance procedures.

4. To integrate a simulated blockchain verification mechanism and a hash-linked audit trail using SHA-256 cryptographic hashing, providing tamper-evident record-keeping and enabling independent verification of document integrity.

5. To develop budget tracking and variance reporting features that allow authorized users to submit, approve, and monitor budget allocations against actual procurement amounts, supporting fiscal accountability.

6. To incorporate compliance monitoring tools, including SLA-based approval thresholds, escalation queues for overdue actions, analytics dashboards, and notification systems, that enable administrators to maintain oversight and enforce procedural standards across the procurement process.

---

## STATEMENT OF THE PROBLEM

The project focused on designing a Municipal Procurement Document Tracking System to support local government units in managing procurement documents with transparency, accountability, and procedural compliance. Specifically, the study will examine the following problems:

1. How can a procurement document tracking system be developed to allow authorized government personnel to upload, review, approve, and publish procurement documents through a structured, role-based workflow that enforces proper governance procedures?

2. In what ways can the system ensure the integrity and authenticity of procurement documents throughout their lifecycle, such that any unauthorized modification can be detected and all actions are recorded in a verifiable, tamper-evident audit trail?

3. How can the system provide citizens and public stakeholders with meaningful access to published procurement documents and the ability to independently verify document authenticity, thereby promoting transparency and public accountability in local government procurement operations?

4. What mechanisms can be implemented to monitor compliance with approval timelines, track budget utilization against allocations, and identify workflow bottlenecks, enabling administrators to maintain oversight and enforce procedural standards across the procurement process?

---

## CURRENT SOLUTION

Existing procurement tracking systems in local government settings generally rely on either manual paper-based processes or basic digital tools such as spreadsheets and shared document folders. These approaches typically allow for the recording of procurement transactions, the storage of supporting documents, and basic status tracking of whether a procurement request has been submitted, reviewed, or completed. Some municipalities have adopted simple database-driven systems that provide a central repository for procurement records, but these remain limited in scope and functionality.

While these solutions address the fundamental need for document storage and retrieval, they present several significant limitations. Paper-based systems are inherently vulnerable to loss, damage, and unauthorized alteration, and they offer no automated mechanism for enforcing approval workflows or maintaining audit trails. Spreadsheet-based approaches, though more accessible, lack role-based access controls, automated routing, and integrity verification features. Even dedicated procurement software solutions available on the market tend to require substantial infrastructure investment, ongoing licensing costs, and technical expertise that many smaller municipalities cannot sustain.

Critically, none of these existing approaches provide an integrated mechanism for cryptographic document verification or tamper-evident audit logging at the local level. The absence of such features means that the authenticity of procurement records cannot be independently verified by auditors or the public, and any modifications made to documents after submission may go undetected. This gap between the governance requirements of procurement transparency and the capabilities of available tools represents the core limitation that current solutions fail to address.

---

## PROPOSED SOLUTION

ProcureChain addresses the limitations of existing procurement tracking approaches by providing an integrated, terminal-based system that combines document lifecycle management with governance enforcement and integrity verification mechanisms — all implemented within a single, self-contained C++ application that requires no external infrastructure, databases, or third-party services.

**What sets ProcureChain apart from existing solutions:**

- **Simulated Blockchain Verification.** The system maintains a dynamic simulated blockchain with five base nodes plus one deterministic node per admin record. Each node independently stores a copy of the chain, enabling cross-node consistency validation and tamper detection. This provides a level of document integrity assurance that no existing spreadsheet-based or simple database-driven procurement system offers at the municipal level.

- **Consensus-Based Approval Workflow.** Unlike traditional systems where a single approver can authorize procurement documents, ProcureChain enforces a unanimous, multi-role consensus mechanism. Documents are routed to designated approvers based on configurable category rules, and publication occurs only when all required roles have approved. A single rejection blocks the entire process, ensuring that no procurement document is published without full governance consensus.

- **Hash-Linked Audit Trail.** Every significant action in the system — from document uploads to approval decisions to budget modifications — is recorded in an append-only audit log where each entry is cryptographically linked to the previous one through SHA-256 hashing. This chain structure ensures that any tampering with historical records is immediately detectable, providing a level of audit integrity that manual or spreadsheet-based logs cannot guarantee.

- **Citizen Verification Capability.** The system provides a public-facing role (Citizen) that can search published procurement documents, view budget summaries, verify document hashes against blockchain records, and browse the audit trail. This direct public access to verification tools is a feature absent from virtually all existing low-cost procurement tracking solutions used by local governments.

- **Zero-Infrastructure Deployment.** ProcureChain operates entirely as a standalone compiled executable with file-based persistence, requiring no database server, web server, network connectivity, or installation of external dependencies. This makes it immediately deployable in resource-constrained municipal environments where IT infrastructure is limited.

- **SLA Monitoring and Governance Analytics.** The system includes built-in compliance monitoring features such as per-category SLA thresholds, escalation queues for overdue approvals, approval throughput analytics, budget variance reporting, and department workload analysis — providing administrators with actionable governance insights that are not typically available in basic document tracking tools.

---

## SCOPE AND DELIMITATION

The scope of this project focuses on developing a console-based municipal procurement document tracking system using C++ as the primary programming language. It aims to apply the fundamental programming concepts covered during the finals period of the academic year 2025–2026, specifically procedural programming techniques including functions, vectors, file I/O, conditional statements, loops, and string manipulation. The system manages the complete procurement document lifecycle — from upload through multi-role approval to public publication — with integrated budget tracking, audit logging, blockchain-based verification, and governance analytics.

The project encompasses the following functional areas: user authentication with SHA-256 password hashing and role-based access control across five roles (Citizen, Super Admin, Procurement Officer, Budget Officer, and Municipal Administrator); document management with metadata, categorization, tagging, and amendment lineage tracking; configurable approval workflows with SLA thresholds and delegated authority; budget entry submission, approval, and variance reporting; an append-only hash-linked audit trail with CSV export capability; a dynamic simulated blockchain with cross-node validation and tamper detection; analytics dashboards for compliance, workload, and performance monitoring; AI-assisted document summarization; and a notification system for pending, overdue, and tamper alerts.

This project is limited to a console-based interface and does not incorporate any graphical user interface (GUI) or web-based frontend. It does not include online features, cloud storage, network communication, or relational database integration; all data is persisted through structured text files that exist locally on the machine where the program is executed. The blockchain implementation is simulated for educational purposes and does not involve actual distributed networking, cryptographic mining, or peer-to-peer consensus protocols. The system does not use external or third-party libraries outside of the standard C++ library, and it does not employ object-oriented programming constructs such as classes or inheritance. Additionally, the project does not focus on comprehensive performance optimization, automated testing frameworks, or formal usability studies. The primary intent is to demonstrate mastery of procedural programming concepts through the construction of a governance-oriented application rather than to create a production-grade procurement management system.

---

## SYSTEM FEATURES

ProcureChain is organized into three tiers of functionality: main features that define the user-facing governance workflow, key technical features that underpin data integrity and search, and supporting features that extend operational utility. Each feature is implemented in procedural C++ without external libraries or object-oriented constructs, demonstrating that governance-oriented system design can be achieved entirely within foundational programming paradigms (Stroustrup, 2014). The following subsections describe each feature group in detail, including the algorithms and data structures employed.

---

### Main Features

#### Document Lifecycle Management

The system supports the complete procurement document lifecycle across six document categories: Purchase Requests, Canvass Forms, Purchase Orders, Notices of Award, Contracts, and Inspection and Acceptance Records. Each document is assigned a unique alphanumeric identifier and transitions through four primary statuses: `pending_approval`, `published`, `rejected`, and `draft`. Document records capture eighteen metadata fields — including title, category, tags, description, department, upload timestamp, uploader identity, SHA-256 hash value, file path, file size, version number, and amendment lineage pointer (`previousDocId`) — and are persisted in a pipe-delimited flat file. Uploaded source files (accepted formats: PDF, DOCX, CSV, TXT) are copied into a local documents directory and hashed on import. Metadata-only uploads are also supported, with the document fingerprint derived from a deterministic serialization of its metadata fields. All status transitions are recorded in a chronological timeline log that captures who changed the status, when, from which state, to which state, and with what notes.

A duplicate hash detection mechanism computes the SHA-256 fingerprint of each incoming file and scans all existing documents for a matching digest. When a collision is found, the system presents the uploader with three options: cancel the upload, continue with the duplicate hash on record, or link the incoming document as an amendment to the existing record. Amendment lineage is maintained via the `previousDocId` field, allowing version chains (v1 → v2 → v3) to be reconstructed at any point.

#### Role-Based Access Control

Access to all system functions is governed by a five-role hierarchy: Citizen, Procurement Officer, Budget Officer, Municipal Administrator, and Super Admin. Navigation menus are dynamically filtered at runtime to display only the actions permitted for the authenticated role, ensuring that unauthorized functions are never reachable through the interface. The Citizen role provides read-only access to published documents, published budget summaries, and document hash verification tools. The Procurement Officer role enables document upload and submission. The Budget Officer and Municipal Administrator roles serve as consensus approvers with additional authority to submit and approve budget entries. The Super Admin role encompasses full system access, including account lifecycle management, manual overrides, blockchain validation, and data backup and restore operations.

#### Multi-Role Unanimous Approval Workflow

Publication of any procurement document requires unanimous approval from all roles designated in the document's category-specific approval rule. Approval routing is governed by a configurable rules file that maps each document category to a set of required approver roles and a maximum decision deadline expressed as an SLA threshold in days. A DEFAULT rule provides fallback routing for uncategorized documents; the default configuration requires both a Budget Officer and a Municipal Administrator with a seven-day decision window. Upon upload, the system automatically generates one pending approval record per required role. Any single rejection immediately transitions the document to rejected status and halts the publication path. Only when all required roles have recorded approved decisions — with no rejections outstanding — does the document transition to published status.

A strict conflict-of-interest guard prevents the original uploader from approving or rejecting their own document, regardless of their assigned role. A request-for-comment (RFC) thread is available for each pending document, enabling approvers to record discussion notes and exchange clarifications before rendering a decision. These comments persist in a dedicated file and are visible to all approvers with access to that document's record.

#### Budget Management and Variance Reporting

Budget entries are submitted through a dedicated Budget Workspace, with each entry capturing fiscal year, category, allocated amount, description, creator identity, and creation timestamp. Budget entries undergo their own unanimous consensus approval workflow before publication. Published budget allocations are aggregated into a summary file and exposed to citizens via a public Budget Summary view.

A variance report compares allocated totals against the sum of approved and published document amounts per category, producing utilization percentages by fiscal period. Budget overrun guardrails enforce a warning threshold at 90% utilization and a hard publication block above 100% on both document and budget publication paths. A monthly transparency report generator produces both a human-readable TXT report and a machine-readable CSV export covering published documents, approval and rejection outcomes, and budget variance by selected YYYY-MM period.

---

### Key Technical Features

#### SHA-256 Cryptographic Hashing

ProcureChain implements the SHA-256 hash function natively in procedural C++, conforming to the Secure Hash Standard as specified by the National Institute of Standards and Technology (2015). SHA-256 is applied for two distinct purposes: password security, where all stored credentials are persisted as 256-bit digest strings rather than plaintext, with automatic migration of any legacy plaintext passwords on startup; and document integrity fingerprinting, where the binary content of an imported source file — or a deterministic serialization of the document's metadata fields when no file is present — is reduced to a 64-character hexadecimal digest that serves as a tamper-evident fingerprint.

The implementation processes input data according to the Merkle-Damgård padding construction: a single `0x80` byte is appended to the message, followed by zero-padding until the total length satisfies `(length mod 64) = 56`, and finally an 8-byte big-endian representation of the original bit length. The padded message is then processed in 512-bit (64-byte) blocks. Each block is expanded into a 64-word message schedule using the recurrence:

```
w[i] = w[i-16] + σ0(w[i-15]) + w[i-7] + σ1(w[i-2])
```

where σ0 and σ1 are right-rotation-based message schedule expansion functions. The 64-round compression function operates on eight 32-bit working variables (a through h), initialized to the fractional-part-of-square-root constants specified in FIPS 180-4, and updated each round using the Ch and Maj bitwise selection functions together with the 64 round constants derived from the fractional parts of cube roots of the first sixty-four prime numbers (National Institute of Standards and Technology, 2015). The final 256-bit digest is produced by concatenating the eight 32-bit hash values as a 64-character lowercase hexadecimal string.

#### Simulated Dynamic-Node Blockchain with Hash-Linking

The blockchain subsystem simulates a distributed ledger by maintaining five independent node files, each storing an identical copy of the append-only chain. This replication model mirrors the redundancy principle of distributed ledger technology, in which multiple independent nodes each retain a full copy of the transaction history to prevent single-point tampering (Bashir, 2020; Nakamoto, 2008). Each block is a pipe-delimited record with seven fields:

```
index | timestamp | action | documentID | actor | previousHash | currentHash
```

The `currentHash` of each block is computed as the SHA-256 digest of the concatenation of all six preceding fields. The `previousHash` of each block equals the `currentHash` of its predecessor, with the genesis block using a fixed placeholder as its `previousHash`. This hash-linking ensures that any retroactive modification of any block's fields would invalidate all subsequent `previousHash` values, making tampering immediately detectable during chain validation.

When a critical action occurs — document upload, approval decision, rejection, or budget publication — the system reads the latest block to determine the next sequential index and the preceding hash, computes the new block's hash, and simultaneously appends the new block row to all active node files. Active nodes follow the formula `5 + total admins` (from admin records), with deterministic admin-linked node naming. Validation cross-checks all active nodes against a reference chain derived from node 1, reporting per-node chain integrity status, the first mismatch index per node, and per-block consensus counts representing the number of nodes that agree on each block's hash value.

#### Full-Text Ranked Search Algorithm

The document search subsystem implements a multi-field weighted scoring algorithm to rank search results by relevance. Given a query string, the system normalizes both the query and each searchable document field to lowercase before comparison. A relevance score is computed for each document using the following additive weight table:

| Match Condition | Score Points |
|---|---|
| Exact match on Document ID | +1200 |
| Document ID begins with query (prefix match) | +700 |
| Document ID contains query (substring match) | +350 |
| Document title contains query | +240 |
| Document description contains query | +170 |
| Document tags contain query | +160 |
| Document category contains query | +130 |
| Per-term match on Document ID (tokenized query) | +120 per term |
| Per-term match on title | +90 per term |
| Per-term match on tags | +80 per term |
| Per-term match on description | +60 per term |
| Per-term match on category | +50 per term |

The query is first tokenized into a set of unique alphanumeric terms. Each term independently contributes its per-term score across all applicable fields, supplementing the single-query score produced by whole-query matching. Documents with a total score of zero are excluded from results. Remaining documents are sorted in descending score order, with ties broken by upload date (newer first) and then by document ID. This field-priority weighting approach is consistent with ranked information retrieval principles in which fields with higher diagnostic specificity receive proportionally greater weight (Manning, Raghavan, & Schütze, 2008).

#### Advanced Multi-Dimensional Filtering Pipeline

The filtering subsystem applies a sequential AND-gate pipeline over the in-memory document collection. A document passes the pipeline only if it satisfies every non-empty criterion simultaneously. The supported filter dimensions and their matching semantics are:

- **Status** — case-insensitive exact equality match against the document status field
- **Exact Date** — exact string match against the upload date in YYYY-MM-DD format
- **Date Range** — inclusive lexicographic range comparison using ISO 8601 zero-padded dates, exploiting the property that lexicographic order equals chronological order for this format
- **Category, Department, Uploader** — case-insensitive substring containment
- **Tags** — case-insensitive substring containment applied independently to each requested tag token; all requested tokens must be present (multi-token AND)

All string comparisons are performed after converting both operands to lowercase via an ASCII-range character transformation. Filter state is encapsulated in a `DocumentFilter` structure, and pre-input suggestions showing the top available values per dimension — aggregated by frequency from the live document set — are displayed before the user enters filter criteria to assist query construction.

#### Hash-Linked Audit Trail

Every significant system action is recorded in an append-only audit log with eleven fields per entry: timestamp, action code, target type, target ID, actor username, actor role, outcome, visibility level, blockchain chain index (populated when the action is also recorded on the blockchain), previous hash, and current hash. Each audit entry's current hash is computed as the SHA-256 digest of the concatenation of the first ten fields, and the previous hash of each entry is the current hash of the immediately preceding entry, with the first entry using an empty string. This creates a hash-linked audit chain structurally analogous to the blockchain, making retroactive insertion, deletion, or modification of any log entry detectable through hash recomputation (Bashir, 2020). The audit log supports full-table rendering, action-frequency analysis, CSV export of all rows, and filtered CSV export by date range, action type, actor, role, or target.

---

### Supporting Features

#### Notification System

On login, the system generates a role-specific notification inbox by scanning the live approval, document, budget, and tamper-alert data files at session start. Administrative roles receive a summary of pending approval requests, overdue items, and recent tamper alerts. The Citizen role receives recently published document updates and public tamper alerts. The inbox is also accessible as a persistent menu option within each role's workspace throughout the session.

#### Analytics Dashboards

 An Analytics Hub provides seven specialized reporting views accessible to administrative roles. The Approval Funnel and Throughput Dashboard displays total, decided, pending, approved, and rejected document counts alongside rejection rates, average decision times, per-role throughput, and SLA compliance rates with bottleneck indicators. The Budget Utilization Report compares allocated and actual amounts by category with utilization charts. The Audit Activity Timeline visualizes action frequency and volume trends over time. The Integrity Status Dashboard summarizes blockchain node health across dynamic active nodes. The Executive Snapshot consolidates high-level governance key performance indicators for rapid review. The Department Workload Report presents approval load, decision latency, and publication coverage broken down by department. The Compliance Audit Report runs a set of rule-based checks against system state and produces a PASS or REVIEW verdict per criterion with a violation listing. An Escalation Queue surfaces all pending approval records that have exceeded their SLA threshold for Super Admin follow-up.

#### Data Backup and Restore

The Super Admin role has access to a Backup and Restore workspace that creates timestamped snapshots of all data files by copying the entire data directory — including all flat files and blockchain node files — into a timestamped backup folder. Restore operations replace the active data directory from a selected backup snapshot. All backup and restore actions are recorded in the audit trail to maintain governance traceability.

#### Delegation Mechanism

The Budget Officer and Municipal Administrator roles can create delegated approval authority assignments, granting a designated delegate the ability to act on their pending approval items during a specified date range. Active delegations surface delegated items in the delegatee's pending approval view alongside directly assigned items. The acting identity is recorded at decision time in the approval record, preserving full traceability of who exercised approval authority even when delegated.

#### In-App Help System

A role-specific help module is accessible from every role's dashboard. Help content explains the key concepts and available actions for that role's scope, including the approval workflow, hash verification process, blockchain integrity checking, and budget consensus procedure. Help is rendered as static in-terminal text without requiring any external documentation resource.

#### Monthly Transparency Reports and CSV Exports

The Budget Workspace provides a monthly transparency report generator producing both a human-readable TXT report and a machine-readable CSV export for a selected YYYY-MM period. Each report includes a listing of all documents published during the period, approval and rejection counts and rates, and a budget variance summary comparing allocated and actual amounts by category. This transparency reporting mechanism supports the open contracting data principles recommended for public procurement systems (Open Contracting Partnership, 2024). A separate CSV export function is available in the Audit Workspace for full and date-range-filtered audit log exports.

---

## REFERENCES

- Commission on Audit. (2023). *Annual audit reports on local government units*. Republic of the Philippines Commission on Audit. https://www.coa.gov.ph

- Congress of the Philippines. (2003). Republic Act No. 9184: Government Procurement Reform Act. *Official Gazette of the Republic of the Philippines*. https://www.officialgazette.gov.ph/2003/01/10/republic-act-no-9184/

- Government Procurement Policy Board. (2023). *Procurement monitoring reports and compliance guidelines*. https://www.gppb.gov.ph

- Bashir, I. (2020). *Mastering blockchain: A deep dive into distributed ledgers, consensus protocols, smart contracts, DApps, cryptocurrencies, Ethereum, and more* (3rd ed.). Packt Publishing.

- Stroustrup, B. (2014). *Programming: Principles and practice using C++* (2nd ed.). Addison-Wesley Professional.

- International Organization for Standardization. (2017). *ISO 37001:2016 — Anti-bribery management systems*. ISO. https://www.iso.org/standard/65034.html

- Open Contracting Partnership. (2024). *Open contracting data standard: Documentation*. https://standard.open-contracting.org

- Transparency International. (2024). *Corruption Perceptions Index 2023*. https://www.transparency.org/cpi2023

- Philippine Government Electronic Procurement System. (2024). *PhilGEPS: About the system*. https://www.philgeps.gov.ph

- National Institute of Standards and Technology. (2015). *Secure hash standard (SHS)* (FIPS Publication 180-4). U.S. Department of Commerce. https://doi.org/10.6028/NIST.FIPS.180-4

- Nakamoto, S. (2008). *Bitcoin: A peer-to-peer electronic cash system*. https://bitcoin.org/bitcoin.pdf

- Manning, C. D., Raghavan, P., & Schütze, H. (2008). *Introduction to information retrieval*. Cambridge University Press. https://nlp.stanford.edu/IR-book/

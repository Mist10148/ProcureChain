<h2 align="center">PROCURECHAIN</h2>
<h4 align="center">Municipal Procurement Document Tracking System</h4>

<br>

---

## PROPOSED TITLE

**A Proposed Terminal-Based Municipal Procurement Document Tracking System with Simulated Blockchain Verification and Role-Based Governance Using C++**

---

## ABSTRACT

This project presents the design of ProcureChain, a terminal-based municipal procurement document tracking system developed using procedural C++ programming. The system provides a structured workflow for uploading, reviewing, approving, and publishing procurement documents within a simulated local government setting. It employs role-based access control across five distinct user roles — Citizen, Super Admin, Procurement Officer, Budget Officer, and Municipal Administrator — each with defined permissions and responsibilities that mirror real-world governance hierarchies.

The core functionality encompasses full document lifecycle management, including multi-role consensus-based approval workflows, budget allocation and variance reporting, and delegated authority mechanisms. To ensure data integrity and transparency, the system integrates SHA-256 cryptographic hashing for password security and document verification, an append-only hash-linked audit trail, and a simulated five-node blockchain that records all critical actions with cross-node consistency validation. Additional features include configurable approval rules with SLA thresholds, ranked full-text search with multi-criteria filtering, analytics dashboards for compliance monitoring and workload analysis, and a notification system for pending and overdue approvals.

All data is persisted through structured, pipe-delimited text files, and the program is built entirely within the standard C++ library without external dependencies or object-oriented constructs. ProcureChain demonstrates how fundamental programming concepts — vectors, functions, file I/O, conditional logic, and modular design — can be applied to construct a governance-oriented system that addresses transparency, accountability, and procedural compliance in municipal procurement operations.

---

## BACKGROUND OF THE STUDY

Public procurement is a critical function of local government, involving the acquisition of goods, services, and infrastructure necessary for delivering public services. In the Philippines, government procurement is governed by Republic Act No. 9184, also known as the Government Procurement Reform Act, which mandates transparency, competitiveness, and accountability in all procurement transactions. Despite these legal frameworks, many local government units (LGUs) continue to rely on manual, paper-based processes for tracking procurement documents, resulting in inefficiencies, delays, and vulnerabilities to irregularities.

According to the Commission on Audit (COA), procurement-related findings consistently rank among the most common audit observations in local government operations. Issues such as missing documentation, incomplete approval chains, unauthorized modifications to procurement records, and lack of proper audit trails have been repeatedly identified as systemic weaknesses. These problems are further compounded by the absence of accessible, low-cost digital tools that smaller municipalities can adopt without requiring extensive IT infrastructure or specialized technical personnel.

The challenge of maintaining document integrity throughout the procurement lifecycle is particularly significant. Once a procurement document is submitted, it must pass through multiple levels of review and approval before it can be considered valid and publishable. Any unauthorized alteration during this process can compromise the legitimacy of the entire transaction. Traditional document management approaches offer limited protection against tampering, as they typically lack mechanisms for verifying whether a document has been modified after submission.

Furthermore, the lack of transparency in procurement processes erodes public trust in local governance. Citizens often have no practical means of verifying whether procurement documents are authentic or whether proper procedures were followed. This information asymmetry between government officials and the public undermines the principles of open governance and participatory democracy that Philippine law seeks to promote.

These challenges highlight the need for a system that can manage the complete procurement document lifecycle — from submission through multi-level approval to publication — while maintaining verifiable records of all actions taken. Such a system should enforce proper workflow procedures, provide tamper-evident audit trails, and enable public verification of document authenticity, all within a framework that is accessible to municipalities with limited technical resources.

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

- **Simulated Blockchain Verification.** The system maintains a five-node simulated blockchain that records all critical procurement actions as hash-linked blocks. Each node independently stores a copy of the chain, enabling cross-node consistency validation and tamper detection. This provides a level of document integrity assurance that no existing spreadsheet-based or simple database-driven procurement system offers at the municipal level.

- **Consensus-Based Approval Workflow.** Unlike traditional systems where a single approver can authorize procurement documents, ProcureChain enforces a unanimous, multi-role consensus mechanism. Documents are routed to designated approvers based on configurable category rules, and publication occurs only when all required roles have approved. A single rejection blocks the entire process, ensuring that no procurement document is published without full governance consensus.

- **Hash-Linked Audit Trail.** Every significant action in the system — from document uploads to approval decisions to budget modifications — is recorded in an append-only audit log where each entry is cryptographically linked to the previous one through SHA-256 hashing. This chain structure ensures that any tampering with historical records is immediately detectable, providing a level of audit integrity that manual or spreadsheet-based logs cannot guarantee.

- **Citizen Verification Capability.** The system provides a public-facing role (Citizen) that can search published procurement documents, view budget summaries, verify document hashes against blockchain records, and browse the audit trail. This direct public access to verification tools is a feature absent from virtually all existing low-cost procurement tracking solutions used by local governments.

- **Zero-Infrastructure Deployment.** ProcureChain operates entirely as a standalone compiled executable with file-based persistence, requiring no database server, web server, network connectivity, or installation of external dependencies. This makes it immediately deployable in resource-constrained municipal environments where IT infrastructure is limited.

- **SLA Monitoring and Governance Analytics.** The system includes built-in compliance monitoring features such as per-category SLA thresholds, escalation queues for overdue approvals, approval throughput analytics, budget variance reporting, and department workload analysis — providing administrators with actionable governance insights that are not typically available in basic document tracking tools.

---

## SCOPE AND DELIMITATION

The scope of this project focuses on developing a console-based municipal procurement document tracking system using C++ as the primary programming language. It aims to apply the fundamental programming concepts covered during the finals period of the academic year 2025–2026, specifically procedural programming techniques including functions, vectors, file I/O, conditional statements, loops, and string manipulation. The system manages the complete procurement document lifecycle — from upload through multi-role approval to public publication — with integrated budget tracking, audit logging, blockchain-based verification, and governance analytics.

The project encompasses the following functional areas: user authentication with SHA-256 password hashing and role-based access control across five roles (Citizen, Super Admin, Procurement Officer, Budget Officer, and Municipal Administrator); document management with metadata, categorization, tagging, and amendment lineage tracking; configurable approval workflows with SLA thresholds and delegated authority; budget entry submission, approval, and variance reporting; an append-only hash-linked audit trail with CSV export capability; a simulated five-node blockchain with cross-node validation and tamper detection; analytics dashboards for compliance, workload, and performance monitoring; and a notification system for pending and overdue actions.

This project is limited to a console-based interface and does not incorporate any graphical user interface (GUI) or web-based frontend. It does not include online features, cloud storage, network communication, or relational database integration; all data is persisted through structured text files that exist locally on the machine where the program is executed. The blockchain implementation is simulated for educational purposes and does not involve actual distributed networking, cryptographic mining, or peer-to-peer consensus protocols. The system does not use external or third-party libraries outside of the standard C++ library, and it does not employ object-oriented programming constructs such as classes or inheritance. Additionally, the project does not focus on comprehensive performance optimization, automated testing frameworks, or formal usability studies. The primary intent is to demonstrate mastery of procedural programming concepts through the construction of a governance-oriented application rather than to create a production-grade procurement management system.

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

# AGENTS.md

## Purpose of this file
This file defines how to work in this repository.

Read this file first.
Then read `README.md` for the high-level functional spec and project intent.
Then read `REQUIREMENTS.md` for the detailed functional requirements.

## Project summary
This repository contains firmware for a Hatlabs Halmet board installed on a boat.

This is an embedded C++ project built with:
- pioarduino
- Arduino framework
- SensESP (firmware library for SignalK)
- SignalK (boat integration specification)

## Build environment
This project uses the pioarduino fork of PlatformIO rather than stock PlatformIO.

Important:
- Do not "normalize" the project back to standard PlatformIO unless explicitly asked.
- When suggesting build, config, or dependency changes, assume the pioarduino environment is intentional.

## Authoritative project references
Use these sources in this order:

1. `AGENTS.md`
   - repository working rules
   - coding behavior
   - change policy
   - clarification policy

2. `README.md`
   - high-level functional spec
   - project intent
   - major behaviors
   - hardware / wiring context
   - current scope


If these sources appear to conflict, stop and ask.

## SensESP guidance
This project uses SensESP concepts and patterns.

###Important
- Use deepwiki MCP to query `SignalK/SensESP` for general knowledge
- The actual SensESP code used is in the `lib/` directory
- `libdeps` references an unused copy of SensESP that should be ignored
- Assume local fork is definitive truth for latest behavior
- Prefer local fork when referencing framework code or proposing framework fixes.
- If a limitation or bug appears to be in SensESP, it is acceptable to identify that clearly and propose a fix that could be made in the user's fork.
- Do not replace SensESP with another framework unless explicitly asked.
- App-specific SensESP-style enhancements in this project, such as extended listeners, custom transforms, or other reusable pipeline components, should use the `app` namespace.
- App-specific classes should mimic the SensESP source tree and be placed in matching project folders such as `src/signalk`, `src/transforms`, `src/system`, or `src/sensors` as appropriate.
- Do not place app-specific classes in the `sensesp` namespace unless the class actually belongs in the SensESP library.
- Do not place app-specific classes in the `halmet` namespace; reserve `halmet` for generic HALMET-related code that could later move into a shared Halmet library.

**CRITIAL** - This project uses the author's version of SensESP found in the `lib/` directory. Since PlatformIO prioritizes code in the `lib` folder, the copy that exists in `.pio/libdeps/halmet/SensESP` can and should be ignored.

## SignalK guidance
- The historical Signal K specification repository is useful for general background, but it is not the authoritative source for current 2.x behavior.
- Treat the Signal K Server source code as the definitive source of truth for current behavior and implementation details.
- When using deepwiki MCP for Signal K questions, prefer `SignalK/signalk-server`.
- Use `SignalK/specification` only as secondary background reference when helpful.

## Hardware context
The target hardware is a Hatlabs Halmet board.

The project runs on a boat, so be mindful of:
- startup reliability
- long-running stability
- noisy electrical environments
- sensor and wiring assumptions
- constrained RAM / flash
- clear handling of fault conditions

External hardware overview reference:
- https://docs.hatlabs.fi/halmet/hardware/

Treat external documentation as reference material.
Treat this repository's files as the primary source for how this project should be structured.

## Structural guidance
Use existing files in this repository as the default template for:
- file organization
- naming conventions
- startup flow
- initialization order
- general architecture

When adding new functionality:
- prefer consistency with existing files in this repository
- avoid introducing a new architecture unless there is a strong reason
- if you believe a structural improvement is warranted, explain it first before making a broad change

## Embedded coding guidance
This code runs on embedded hardware. Optimize for reliability and clarity.

Prefer:
- simple, readable C++
- small functions
- straightforward control flow
- explicit hardware assumptions
- code that is easy to bench test

Avoid unless clearly justified:
- unnecessary abstraction
- dependency churn
- gratuitous dynamic allocation
- unnecessary heap churn in steady-state code
- excessive use of dynamically built `String` values in hot or repeated paths

## Change policy
Keep changes minimal and local.

Rules:
- update only the files needed for the task
- preserve existing naming and structure where practical
- do not change PlatformIO environments, board settings, or major dependency choices unless the task requires it
- do not introduce a new persistence, transport, configuration, or architectural approach without confirming first if the choice is material
- prefer the simplest implementation that matches existing project patterns

## Clarification policy
Do not guess about requirements that materially affect architecture, maintainability, or integration.

Stop and ask concise clarifying questions before coding when:
- there are multiple reasonable architectural choices
- persistence, transport, storage, or configuration strategy is unspecified and the choice matters
- hardware or wiring assumptions are missing and they affect the implementation
- the request conflicts with earlier instructions, the README, or existing project conventions
- the requested change implies a non-trivial refactor
- a dependency change or framework change may be required
- there is more than one reasonable way to implement the feature and the choice would matter to the user

If the task is straightforward and the missing details do not materially affect the result:
- proceed with the simplest option consistent with the existing codebase
- state the assumption briefly

When asking questions:
- prefer 1 to 3 focused related questions at a time
- do not produce a long questionnaire
- if more clarification is needed, ask in multiple short rounds rather than all at once
- resolve high-impact uncertainties first before asking lower-impact follow-ups
- ask before coding, not after making avoidable architectural choices

When requirements are ambiguous:
- prefer consistency with existing files in this repository
- but ask before making a significant deviation from that pattern

## Output preferences
When responding about code changes:
- be concise
- describe only the files changed and why
- mention important assumptions
- call out anything that should be bench-tested or boat-tested

If there are multiple reasonable approaches:
- choose the simplest one that matches the existing code structures and patterns
- unless the user asks to compare alternatives first

## Testing expectations
When practical:
- verify that the code is consistent with the PlatformIO project structure
- call out any compile-time or hardware-test risks
- clearly distinguish between code that is syntactically reasonable and code that still requires bench or on-boat validation

# DeepWiki MCP
- DeepWiki MCP is a tool that can be used to search for information in the DeepWiki documentation
- It is useful for finding information about the Signal K protocol and the SensESP framework
- When starting new large tasks, see if it is installed and accessible to you.  If not, stop and ask user if they would like to install it to assist with the task

## Practical working sequence
For most substantial tasks, use this order:
1. Read `AGENTS.md`
2. Read `README.md`
3. Inspect existing files in this repository
4. Ask clarifying questions if a material decision is unresolved
5. Make minimal changes aligned with the existing local pattern
6. Summarize changed files, assumptions, and recommended testing

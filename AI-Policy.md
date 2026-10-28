# AI Policy And Project Declaration

> **KelpOS uses AI assistance, and this project identifies its approach as
> Vibecoded OSDev, or V-OSDev.**

This is an intentional and transparent part of how KelpOS is being developed.
AI tools can be used to explore designs, explain unfamiliar topics, draft or
revise code, investigate errors, write tests, and improve documentation. The
amount and kind of assistance can vary from task to task.

## What V-OSDev Means Here

**V-OSDev** stands for **Vibecoded OSDev**: operating-system development where
the creator freely uses AI tools to brainstorm, learn, write, debug, and improve
the project. The work is still OS development; AI assistance is part of the
workflow, not a reason to pretend the project was made without it.

Nimbus32 describes KelpOS as still OSDev, but mostly vibecoded. V-OSDev is an
informal approach anyone can use, whether they are an experienced systems
programmer or just starting out and excited to make an operating system. You do
not need to be an expert before you begin. The point is to learn by building,
ask for help when useful, and be honest about what you made, what AI helped
with, and what actually works.

### A Declaration Against Gatekeeping

Using AI for much or even most of a project does not make its creator a
"larper." They may be new to OS development, curious about how operating
systems work, or using AI to get through concepts they do not fully understand
yet. OSDev can look extremely difficult when you are starting out, and everyone
starts somewhere. Do not put someone down simply because they used AI to learn
or help build something. Encourage them to keep learning, ask questions, and
check how their system works; understanding can grow along with the project.

Calling something V-OSDev is not a technical standard, a measure of skill, or a
promise that generated code is correct. It is a playful name for an
AI-assisted, learn-as-you-go style of OS development. People may use the term
for their own projects if it describes their process.

Nimbus32 is the project creator and maintainer of KelpOS. Project goals, priorities,
acceptance of changes, and release decisions remain under the maintainer's
direction. AI is a tool used during that work; it is not the project's owner or
an authority whose output should be accepted without question.

## Review And Verification

AI-generated or AI-assisted work can be incomplete, incorrect, insecure, or
inconsistent with the project's design. Before treating a change as working,
the maintainer should review it in context and run the relevant build, test, or
runtime checks available for that component. A passing check is evidence for
that specific behavior, not proof that the whole operating system is reliable.

When a component cannot yet be run inside KelpOS, such as a host-side prototype
developed before the kernel has a userspace runtime, its status and test boundary
should be stated clearly. Planned features should not be presented as completed
features.

## Attribution And Responsibility

Project descriptions and contribution notes should disclose meaningful AI
assistance. Do not present AI-assisted work as entirely unaided, and do not
describe the whole project as mostly AI-made unless that is an accurate account
of its development. Credit and responsibility should be specific to the work
being discussed.

The maintainer remains responsible for decisions about code quality, security,
licensing, dependencies, and publication. AI assistance does not transfer that
responsibility or guarantee that generated material is correct or appropriately
licensed.

## Project Status

KelpOS is an independent learning and OS-development project. Calling it
**V-OSDev** is a straightforward disclosure of its AI-assisted workflow, not an
apology and not a substitute for documenting how its systems work. The project
will continue to identify what is implemented, what is tested, and what remains
planned.

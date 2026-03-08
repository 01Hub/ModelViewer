# Security Policy

## Overview

ModelViewer is a community-driven, open-source 3D visualization tool maintained as a hobby project. While I can't guarantee enterprise-level security support, I do take security seriously and appreciate the community's help in keeping ModelViewer secure.

---

## Reporting Security Issues

### Please Don't Create Public Issues

If you find a security vulnerability, **don't open a public GitHub issue**. Instead, email me so we can fix it before it becomes public knowledge.

### How to Report

**Email:** [sharjith@gmail.com](mailto:sharjith@gmail.com)

**Subject:** `[SECURITY] Vulnerability Report`

### What to Include

Please provide:

1. **What's the issue?** - Brief description
2. **How to reproduce?** - Steps to see the problem
3. **Which versions?** - Does it affect the latest release?
4. **Why is it bad?** - What's the potential impact?
5. **Your fix?** (optional) - Any ideas on fixing it?

**Example:**

```
Subject: [SECURITY] Crash when loading malformed STEP files

Issue: The STEP parser doesn't validate certain data, causing crashes.

Steps to reproduce:
1. Open the attached STEP file
2. App crashes with segmentation fault

Versions: Happens in 1.0, probably older versions too

Impact: Someone could share a malicious STEP file to crash the app

Fix idea: Maybe add bounds checking in the parser?
```

---

## What to Expect

**Timeline:**
- I'll read your email ASAP
- I'll try to reproduce it within a week
- Fix time depends on severity and my availability
- When fixed, I'll release a new version
- You'll be credited if you want

**Be realistic:**
- This is a hobby project maintained by one person
- Critical issues get priority
- Less critical ones might take a few weeks
- I'll keep you updated on progress

---

## Supported Versions

I only support the **latest release**. Always update to the newest version to get fixes.

| Version | Status |
|---------|--------|
| Latest  | ✅ Gets fixes |
| Older   | ❌ No updates |

---

## Security Considerations

### What ModelViewer Is

- A tool for viewing 3D models
- Loads common 3D file formats (OBJ, FBX, STEP, glTF, etc.)
- Provides visualization with lighting and materials

### What ModelViewer Is NOT

- A file validator (don't trust random 3D files from the internet)
- A security tool
- Designed to run untrusted code safely
- Responsible for validating dependencies (Qt, Assimp, etc. are their own projects)

### Plain English

ModelViewer is **open source software provided as-is**. It works well for most files, but:

- If someone makes a malicious 3D file, it might crash
- I can't promise it's 100% secure
- If you load files from untrusted sources, that's your risk
- The libraries we use (Qt, Assimp, OpenCASCADE) have their own security concerns

---

## Tips for Users

**If you load files from untrusted sources:**

1. Use the latest version
2. Test new files in a virtual machine if you're worried
3. Don't give ModelViewer unnecessary file access
4. Keep your OS and dependencies updated

**Report suspicious behavior:**

If something seems off, email me with details. Even small oddities can help us spot issues.

---

## Dependencies

ModelViewer uses several well-maintained libraries:

- **Qt** - GUI framework (actively maintained)
- **Assimp** - 3D file loading (actively maintained)
- **OpenCASCADE** - CAD support (actively maintained)
- **GLM** - Math library (stable)

I keep these updated, but they're separate projects with their own security concerns. If you find issues in them, report to the respective projects:

- **Qt**: [qt.io](https://www.qt.io/contact-us)
- **Assimp**: [github.com/assimp/assimp](https://github.com/assimp/assimp)
- **OpenCASCADE**: [opencascade.com](https://www.opencascade.com/)

---

## Questions?

Have security suggestions or ideas? Feel free to email or open a discussion. I'm always interested in hearing about ways to improve.

---

## License

ModelViewer is licensed under GPL-3.0. See the LICENSE file for complete terms.

**In plain English:** Use and modify as you like, but you're responsible for your own use of it.

---

**Last Updated:** March 2026

# Contributing to ModelViewer

Thank you for your interest in contributing to ModelViewer! We welcome contributions from the community, whether it's bug reports, feature requests, documentation improvements, or code contributions. This document provides guidelines and instructions for contributing to the project.

---

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Getting Started](#getting-started)
3. [Types of Contributions](#types-of-contributions)
4. [How to Report Bugs](#how-to-report-bugs)
5. [How to Suggest Features](#how-to-suggest-features)
6. [Development Setup](#development-setup)
7. [Coding Standards](#coding-standards)
8. [Git Workflow](#git-workflow)
9. [Submitting Changes](#submitting-changes)
10. [Pull Request Process](#pull-request-process)
11. [Testing](#testing)
12. [Documentation](#documentation)
13. [License](#license)

---

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inspiring community for all. Please read and adhere to our Code of Conduct:

- **Be Respectful**: Treat all contributors with respect and courtesy
- **Be Inclusive**: Welcome diverse perspectives and backgrounds
- **Be Constructive**: Provide helpful, actionable feedback
- **Be Professional**: Keep discussions focused on the project
- **Report Issues**: Use private channels to report violations

Unacceptable behavior includes harassment, discrimination, intimidation, and unwelcoming communication. Any violations will be addressed promptly.

---

## Getting Started

### Prerequisites

Before contributing code, ensure you have:

- **Git** installed and configured
- **CMake** 3.15 or later
- **Qt 6.8** or later
- **Assimp 5.0.1** or later
- **OpenCASCADE 7.9** or later
- **OpenGL 4.6** compatible GPU
- A GitHub account

### Setting Up Your Development Environment

1. **Fork the repository**
   ```bash
   # Visit https://github.com/sharjith/ModelViewer-Qt and click "Fork"
   ```

2. **Clone your fork**
   ```bash
   git clone https://github.com/YOUR-USERNAME/ModelViewer-Qt.git
   cd ModelViewer-Qt
   ```

3. **Add upstream remote**
   ```bash
   git remote add upstream https://github.com/sharjith/ModelViewer-Qt.git
   ```

4. **Install dependencies** (see README.md for platform-specific instructions)

5. **Create a build directory**
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   make
   ```

6. **Run tests** (if available)
   ```bash
   # Tests will be in the build directory
   ```

---

## Types of Contributions

### 🐛 Bug Reports
Help us identify and fix issues in the code.

### ✨ Feature Requests
Suggest enhancements and new functionality.

### 📚 Documentation
Improve README, API docs, comments, and guides.

### 🔧 Code Contributions
Fix bugs, implement features, and improve code quality.

### 💡 Ideas & Discussion
Share insights and participate in design discussions.

### 🎨 Examples & Tutorials
Create sample projects and learning materials.

---

## How to Report Bugs

### Before Submitting a Bug Report

- **Check existing issues**: Search [GitHub Issues](https://github.com/sharjith/ModelViewer-Qt/issues) to avoid duplicates
- **Try the latest version**: The bug may already be fixed
- **Check documentation**: Verify it's not expected behavior
- **Isolate the problem**: Create a minimal reproducible example

### Submitting a Bug Report

When creating a bug report, include:

**Title**: Clear, descriptive summary
```
[BUG] Application crashes when loading STEP file with assemblies
```

**Description**: What you were doing when the bug occurred

**Steps to Reproduce**:
```
1. Open ModelViewer
2. Load a STEP file with nested assemblies
3. Expand the assembly tree
4. Application crashes
```

**Expected Behavior**: What should have happened

**Actual Behavior**: What actually happened

**Environment**:
```
- OS: Linux / Windows / macOS
- OS Version: (e.g., Ubuntu 22.04, Windows 11)
- Qt Version: 6.8.0
- Assimp Version: 5.2.0
- OpenCASCADE Version: 7.9.0
- GPU: [your GPU model]
- GPU Driver: [driver version]
```

**Attachments**:
- Screenshot or screen recording
- Model file (if possible, provide a minimal test case)
- Console/log output
- Crash dump (if available)

**Additional Context**: Any other relevant information

---

## How to Suggest Features

### Before Submitting a Feature Request

- **Check existing issues**: Avoid duplicate requests
- **Consider compatibility**: Does it fit the project's vision?
- **Think about implementation**: Is it technically feasible?

### Submitting a Feature Request

**Title**: Clear description of the feature
```
[FEATURE] Add support for glTF 2.1 extensions
```

**Description**: Explain the feature and why it would be useful

**Motivation**:
```
Many modern 3D models use glTF 2.1 extensions. Adding support would:
- Enable visualization of newer content
- Improve professional workflow compatibility
- Align with Khronos standards
```

**Proposed Solution**: How you envision implementing it (optional)

**Alternatives Considered**: Other approaches you've thought of

**Additional Context**: Use cases, examples, references

---

## Development Setup

### Building from Source

**Linux:**
```bash
git clone https://github.com/sharjith/ModelViewer-Qt.git
cd ModelViewer-Qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
./ModelViewer
```

**Windows (with vcpkg):**
```bash
git clone https://github.com/sharjith/ModelViewer-Qt.git
cd ModelViewer-Qt
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
.\Debug\ModelViewer.exe
```

**macOS:**
```bash
git clone https://github.com/sharjith/ModelViewer-Qt.git
cd ModelViewer-Qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
make -j$(sysctl -n hw.ncpu)
./ModelViewer.app/Contents/MacOS/ModelViewer
```

### Project Structure

```
ModelViewer-Qt/
├── src/                    # Source code
├── include/                # Headers
├── ui/                     # Qt UI files
├── shaders/                # GLSL shader files
├── res/                    # Icons and images
├── textures/               # Textures for materials, env maps, etc.
│    ├── envmap/            # Env maps
│    ├── materials/         # Material textures
│    ├── patterns/          # Hatch patterns
│
├── themes/                 # UI theme qss files
├── translations/           # Qt translations
├── tools/                  # Blender scripts used for generating the default skybox cubemap
├── data/                   # Application Data
│    ├── catalogs/          # Material Catalogs
│    ├── tutorials/         # Tutorial HTML files
│
├── fonts/                  # Application fonts
├── cmake/                  # CMake modules
├── docs/                   # Documentation
├── CMakeLists.txt          # Build configuration
├── README.md               # Project overview
├── CONTRIBUTING.md         # This file
└── LICENSE                 # GPL-3.0 license
```

---

## Coding Standards

### C++ Guidelines

**Language Standard**: Modern C++17

**Style Conventions**:
```cpp
// Classes and types: PascalCase
class MaterialEditor { };
struct RenderingContext { };

// Functions and methods: camelCase
void loadModel(const std::string& filePath);
glm::vec3 calculateNormal(const Triangle& tri);

// Constants: UPPER_CASE
constexpr float PI = 3.14159f;
const int MAX_TEXTURES = 16;

// Member variables: _ prefix
class Renderer
{
private:
    float _exposure;
    GLuint _framebuffer;
};

// Local variables: camelCase
glm::vec3 viewDirection = glm::normalize(camera.position - targetPoint);
```

**Code Quality Rules**:

1. **Follow RAII principles**: Resources should be acquired in the constructor and released in the destructor
   ```cpp
   class TextureManager
   {
   public:
       TextureManager() { /* allocate */ }
       ~TextureManager() { /* deallocate */ }
   private:
       std::vector<GLuint> textures;
   };
   ```

2. **Use smart pointers**: Prefer `std::unique_ptr` and `std::shared_ptr` over raw pointers
   ```cpp
   // Good
   std::unique_ptr<Material> material = std::make_unique<Material>();
   
   // Avoid
   Material* material = new Material();
   ```

3. **Error handling**: Use exceptions or return error codes
   ```cpp
   try
   {
       model = loadModel(filePath);
   } catch (const std::runtime_error& e)
   {
       logger.error("Failed to load model: " + std::string(e.what()));
   }
   ```

4. **Const correctness**: Mark methods as const when they don't modify state
   ```cpp
   glm::mat4 getTransform() const { return m_transform; }
   void setTransform(const glm::mat4& t) { m_transform = t; }
   ```

5. **Comments**: Write clear, meaningful comments
   ```cpp
   // Cook-Torrance BRDF calculation with energy conservation
   float ggxDistribution(float roughness, float nDotH);
   
   // Inverse-transpose matrix for correct normal transformation
   glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
   ```

### GLSL Shader Guidelines

```glsl
// Shader structure
#version 460 core

// Uniforms (updated per draw call or less frequently)
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

// Input variables (from vertex buffer)
in vec3 aPosition;
in vec3 aNormal;
in vec2 aTexCoord;

// Output variables
out vec3 vNormal;
out vec2 vTexCoord;

// Shader logic
void main()
{
    vNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vTexCoord = aTexCoord;
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
```

**Shader Best Practices**:
- Use meaningful variable names (avoid `a`, `b`, `c`)
- Include version and feature requirements at the top
- Comment complex calculations (e.g., BRDF, matrix transforms)
- Use consistent indentation (4 spaces)

### Documentation Standards

**Header Comments**:
```cpp
/**
 * @brief Load a 3D model from disk using Assimp
 * @param filePath Path to the model file (STEP, IGES, OBJ, FBX, etc.)
 * @return Loaded model or nullptr on failure
 * @throws std::runtime_error If the file cannot be opened or parsed
 * 
 * Supports all Assimp formats plus OpenCASCADE native formats.
 * Assembly hierarchies and material metadata are preserved.
 */
std::unique_ptr<Model> loadModel(const std::string& filePath);
```

**Inline Comments**:
```cpp
// Determinant-based detection for negative scale (flipped geometry)
float determinant = glm::determinant(glm::mat3(modelMatrix));
if (determinant < 0.0f)
{
    // Invert normals to face the correct direction
    normal = -normal;
}
```

---

## Git Workflow

### Branch Naming

Use descriptive branch names:
```
feature/khr-transmission          # New feature
bugfix/negative-scale-normals     # Bug fix
docs/shader-documentation         # Documentation
refactor/texture-caching          # Code refactoring
test/material-editor              # Tests
```

### Commit Messages

Write clear, concise commit messages following this format:

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Examples**:
```
feat(material): implement KHR_materials_transmission with refraction

- Add per-channel IOR support
- Implement screen-space ray tracing for refraction
- Add mipmapped environment map sampling

Closes #123
```

```
fix(export): correct material deduplication in GLB pipeline

The material matching was using an index-based comparison instead of
name-based matching, causing Assimp reordering to break assignments.

Now uses name-based matching with proper sampler comparison.

Fixes #456
```

**Commit Types**:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Code style changes (formatting, missing semicolons, etc.)
- `refactor`: Code refactoring without feature or bug changes
- `perf`: Performance improvement
- `test`: Adding or updating tests
- `chore`: Build tools, dependencies, etc.

---

## Submitting Changes

### Step 1: Create a Feature Branch

```bash
git checkout -b feature/your-feature-name
```

### Step 2: Make Changes

- Keep commits atomic and focused
- One logical change per commit
- Write descriptive commit messages

### Step 3: Keep Your Branch Updated

```bash
git fetch upstream
git rebase upstream/master
```

### Step 4: Push to Your Fork

```bash
git push origin feature/your-feature-name
```

### Step 5: Create a Pull Request

Visit GitHub and create a PR from your branch to `upstream/master`.

---

## Pull Request Process

### Before Submitting

- [ ] Code follows project style guidelines
- [ ] All commits are properly formatted
- [ ] Branch is up to date with `upstream/master`
- [ ] Code compiles without warnings
- [ ] Changes are tested locally
- [ ] Documentation is updated

### PR Template

```markdown
## Description
Brief description of the changes

## Motivation
Why are these changes needed?

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation
- [ ] Performance improvement
- [ ] Code refactoring

## Related Issues
Closes #123

## Testing
How were changes tested?

## Screenshots (if applicable)
Before/after images for UI changes

## Checklist
- [ ] My code follows the style guidelines
- [ ] I have performed a self-review
- [ ] I have commented on complex areas
- [ ] My changes generate no new warnings
- [ ] I have added/updated tests
- [ ] New dependencies are documented
```

### PR Review Process

1. **Maintainer Review**: Code is reviewed for quality, functionality, and alignment with project goals
2. **Feedback**: Suggestions or requested changes will be provided
3. **Updates**: Make requested changes in new commits (don't force-push)
4. **Approval**: PR is approved once all feedback is addressed
5. **Merge**: Maintainer merges the PR using the appropriate strategy

**Review Criteria**:
- Code quality and style compliance
- Functionality correctness
- Performance impact
- Documentation completeness
- Test coverage
- Breaking changes disclosure

---

## Testing

### Unit Tests

If adding new functionality, include unit tests:

```cpp
#include <gtest/gtest.h>
#include "material.h"

TEST(MaterialTest, KHRTransmissionParsing)
{
    // Test implementation
    EXPECT_TRUE(material.hasTransmission());
    EXPECT_FLOAT_EQ(material.getIOR(), 1.5f);
}
```

### Integration Tests

Test with real 3D models:
- STEP files with assemblies
- glTF models with KHR extensions
- OBJ files with textures
- FBX models with animations

### Build Testing

Test on multiple platforms:
```bash
# Linux (GCC)
# Windows (MSVC)
# macOS (Clang)
```

### Performance Testing

Profile changes to ensure no regression:
```bash
# Measure shader compilation time
# Profile material editor responsiveness
# Check texture loading performance
```

---

## Documentation

### Code Documentation

- Add Doxygen-style comments to public APIs
- Explain complex algorithms
- Include references to papers/specifications

### README Updates

Update README.md if you:
- Add/remove dependencies
- Change build process
- Add major features
- Fix documented bugs

### Shader Documentation

Comment on non-obvious calculations:
```glsl
// Frisvad orthonormal basis for importance sampling
// Reference: "Building an Orthonormal Basis, Revisited"
vec3 frisvadBasis(vec3 normal);
```

### Commit Documentation

Keep commit messages clear for future reference:
```
This helps with:
- Git log readability
- Bisect debugging
- Historical understanding
```

---

## Performance Considerations

### When Contributing Graphics Code

1. **Profile changes**: Ensure no frame rate regression
2. **Optimize GPU memory**: Minimize texture uploads
3. **Batch rendering**: Reduce draw calls
4. **Shader optimization**: Avoid expensive operations in loops

### When Contributing Export Features

1. **Test large models**: 100+ MB files
2. **Memory efficiency**: Don't load entire files into memory
3. **Progress feedback**: Long operations should report progress

---

## Reporting Security Issues

**Do not create public issues for security vulnerabilities.**

Instead, email [sharjith@gmail.com](mailto:sharjith@gmail.com) with:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if available)

We'll acknowledge receipt within 48 hours and work with you on a fix.

---

## Questions & Support

- **GitHub Issues**: For bugs and feature requests
- **GitHub Discussions**: For questions and design discussions
- **Email**: [sharjith@gmail.com](mailto:sharjith@gmail.com) for security issues

---

## License

By contributing to ModelViewer, you agree that your contributions will be licensed under its GPL-3.0 License.

---

## Acknowledgments

Thank you for contributing to ModelViewer! Your efforts help make professional 3D visualization tools better for everyone.

**Happy coding!** 🚀✨

---

**Last Updated**: March 2026  
**Version**: 1.0

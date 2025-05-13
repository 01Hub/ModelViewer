# ModelViewer
OpenGL Model Viewer

A 3D Viewer that reads and displays most common 3D file formats that are supported by the Assimp library.

![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Screenshot%202021-07-24%20172616.jpg)

It supports various visualization and rendering styles including Physically Based Rendering as seen in the screenshots below.

Rendering Modes
![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Slide1.PNG)

Advanced Rendering Modes
![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Slide2.PNG)

OBJ File Rendering with Textures and Colors
![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Slide3.PNG)

Transparency and Reflections
![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Screenshot%202021-08-06%20150032.jpg)

Material Rendering with Texture Based PBR
![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Slide4.PNG)

Realistic Rendering with Skybox Environment
![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Slide5.PNG)

Capped Section View
![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Slide6.PNG)

Capped Multiple Section View
![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Slide7.PNG)

Multiple Projection Views
![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Screenshot%202021-07-24%20223138.jpg)

Building the Code:
Prerequisites

Ensure you have the following installed:

    CMake: Version 3.15 or above
    Qt: Version 6.8 or above
    Assimp: Version 5.0.1
    GLM
    Freetype: Version 2.10.1
    OpenCascade: Version 7.9
    vcpkg (for Windows)

Build Instructions
Linux

    Install the required dependencies using your package manager (e.g., apt, yum, etc.).
        Ensure all dependencies (listed above) are available in your system.
    Clone the repository:


    git clone https://github.com/sharjith/ModelViewer-Qt.git
    cd ModelViewer-Qt

Create a build directory and configure the project using CMake:
    
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release

Build the project:

    make

Run the application:

    ./ModelViewer   

Windows

    Install vcpkg if not already installed. Follow the instructions here.
    Use vcpkg to install the required dependencies:

    vcpkg install qt6-base assimp glm freetype:x64-windows opencascade:x64-windows

Clone the repository:

    git clone https://github.com/sharjith/ModelViewer-Qt.git
    cd ModelViewer-Qt

Create a build directory and configure the project using CMake with the vcpkg toolchain:

    mkdir build
    cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release

Replace [path_to_vcpkg] with the path to your vcpkg installation.
Build the project:

    cmake --build . --config Release

Run the application:

    .\Release\ModelViewer.exe

Additional Notes

    Replace the library versions with the latest ones, if applicable, but ensure compatibility with the project.
    For troubleshooting, refer to the documentation of the respective tools and dependencies. On Windows, you can open the project using CMake option and build.


# ModelViewer
OpenGL Model Viewer

A 3D Viewer that reads and displays the most common 3D file formats that are supported by the Assimp library and STEP and IGES files using the Open Cascade library.

![ScreenShot](https://github.com/sharjith/ModelViewer/blob/master/screenshots/Screenshot%202021-07-24%20172616.jpg)

It supports various visualization and rendering styles, including Physically Based Rendering, as seen in the screenshots below.

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

Khronos KHR Materials Extensions
<img width="1280" height="720" alt="KHR_1" src="https://github.com/user-attachments/assets/8172c364-957d-4111-9004-824c833cee5e" />
<img width="1280" height="720" alt="KHR_2" src="https://github.com/user-attachments/assets/199321ed-c816-4f36-b6b2-d965cc31f47d" />
<img width="1280" height="720" alt="KHR_3" src="https://github.com/user-attachments/assets/49bf6b81-62b1-4b9f-b454-17e6c9492b2c" />
<img width="1280" height="720" alt="KHR_4" src="https://github.com/user-attachments/assets/0ddce941-d7a5-4e9d-accb-851ce50dace6" />
<img width="1280" height="720" alt="KHR_5" src="https://github.com/user-attachments/assets/bccf4eeb-36f9-404d-bd2f-c3f9df33e21e" />
<img width="1280" height="720" alt="KHR_6" src="https://github.com/user-attachments/assets/2de7a2c4-bb37-4305-b33a-88448653bd1b" />
<img width="1280" height="720" alt="KHR_7" src="https://github.com/user-attachments/assets/5d1f7be9-d423-4e94-a46c-a4c4371b1705" />
<img width="1280" height="720" alt="KHR_8" src="https://github.com/user-attachments/assets/de876c17-ee6d-4aff-8e7e-38225b761ffa" />

Realistic Automotive Paint Rendering
<img width="1920" height="1032" alt="Screenshot 2026-03-02 213714" src="https://github.com/user-attachments/assets/58f6ae47-3e7d-41b6-aa56-362c037fcd4c" />



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


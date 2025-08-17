#include "MaterialPreviewWidget.h"
#include <vector>
#include <cmath>

MaterialPreviewWidget::MaterialPreviewWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(200, 200);
}

MaterialPreviewWidget::~MaterialPreviewWidget()
{
    makeCurrent();
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    doneCurrent();
}

void MaterialPreviewWidget::setMaterial(const GLMaterial &mat)
{
    currentMaterial = mat;
    update();
}

void MaterialPreviewWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);

    shader.addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;

        uniform mat4 uMVP;
        uniform mat4 uModel;
        uniform mat3 uNormalMatrix;

        out vec3 vNormal;
        out vec3 vPos;

        void main() {
            vPos = vec3(uModel * vec4(aPos,1.0));
            vNormal = normalize(uNormalMatrix * aNormal);
            gl_Position = uMVP * vec4(aPos,1.0);
        }
    )");

    shader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
        #version 450 core
        in vec3 vNormal;
        in vec3 vPos;
        out vec4 FragColor;
        uniform vec3 uCamPos;
        uniform vec3 uAlbedo;
        uniform float uMetalness;
        uniform float uRoughness;
        uniform float uOpacity;        
        uniform float uClearcoat;
        uniform float uClearcoatRoughness;
        uniform vec3  uSheenColor;
        uniform float uSheenRoughness;
        uniform float uTransmission;
        uniform float uIOR;
        uniform float uSpecular;

        void main()
        {
            // Clamp uniforms to safe ranges to handle uninitialized values
            vec3 albedo = clamp(uAlbedo, vec3(0.01), vec3(1.0)); // Prevent pure black
            float metalness = clamp(uMetalness, 0.0, 1.0);
            float roughness = clamp(uRoughness, 0.01, 1.0); // Prevent division by zero
            float opacity = clamp(uOpacity, 0.0, 1.0);
            float clearcoat = clamp(uClearcoat, 0.0, 1.0);
            float clearcoatRoughness = clamp(uClearcoatRoughness, 0.01, 1.0);
            vec3 sheenColor = clamp(uSheenColor, vec3(0.0), vec3(1.0));
            float transmission = clamp(uTransmission, 0.0, 1.0);
            float ior = clamp(uIOR, 1.0, 3.0); // Reasonable IOR range
    
            vec3 N = normalize(vNormal);
            vec3 V = normalize(uCamPos - vPos);
    
            // Check for invalid vectors
            if (length(N) < 0.1) N = vec3(0.0, 1.0, 0.0); // Default up vector
            if (length(V) < 0.1) V = vec3(0.0, 0.0, 1.0); // Default view vector
    
            // --- Simple 3-light rig ---
            vec3 L1 = normalize(vec3( 0.5,  0.7, 0.5));
            vec3 L2 = normalize(vec3(-0.4,  0.3, 0.7));
            vec3 L3 = normalize(vec3( 0.0, -0.8, 0.6));
            vec3 lights[3] = {L1, L2, L3};
            vec3 lightColors[3] = {
                vec3(1.0, 1.0, 1.0),
                vec3(0.6, 0.6, 0.7),
                vec3(0.4, 0.4, 0.5)
            };

            
            // --- Metal vs dielectric base colors (smooth blend) ---
            vec3 dielectricDiffuse = albedo;
            vec3 dielectricSpecular = vec3(0.04);
            vec3 metallicDiffuse = vec3(0.0);
            vec3 metallicSpecular = albedo;

            vec3 diffuseColor = mix(dielectricDiffuse, metallicDiffuse, metalness);
            vec3 specularColor = mix(dielectricSpecular, metallicSpecular, metalness);

            // --- Calculate lighting ---
            vec3 color = vec3(0.0);
            // --- Loop over lights ---
            for (int i = 0; i < 3; i++) {
                vec3 L = lights[i];
                vec3 H = normalize(V + L);
                float NdotL = max(dot(N, L), 0.0);
                float NdotH = max(dot(N, H), 0.0);
                // Simple specular (Blinn-Phong-like)
                float specPow = mix(8.0, 128.0, 1.0 - roughness);
        
                // Clamp specular power and factor to prevent extreme values
                specPow = clamp(specPow, 1.0, 256.0);
                float specFactor = pow(clamp(NdotH, 0.0, 1.0), specPow);
                specFactor = clamp(specFactor, 0.0, 10.0); // Prevent extreme highlights
        
                vec3 diffuse = diffuseColor * NdotL;
                vec3 spec = specularColor * specFactor;
        
                // Clamp intermediate results
                diffuse = clamp(diffuse, vec3(0.0), vec3(2.0));
                spec = clamp(spec, vec3(0.0), vec3(5.0));
        
                color += (diffuse + spec) * lightColors[i] * NdotL;
            }

            // --- Ambient fallback ---
            vec3 dielectricAmbient = albedo * 0.2;
            vec3 metallicAmbient = albedo * 0.1;
            vec3 ambient = mix(dielectricAmbient, metallicAmbient, metalness);

            color += ambient;

            // --- Clearcoat ---
            if (clearcoat > 0.001) {
                vec3 Hc = normalize(V + L1);
                float ccNdotH = max(dot(N, Hc), 0.0);
                float ccSpecPow = mix(64.0, 512.0, 1.0 - clearcoatRoughness);
                ccSpecPow = clamp(ccSpecPow, 1.0, 1024.0); // DEFENSIVE: Clamp power
        
                float ccSpec = pow(clamp(ccNdotH, 0.0, 1.0), ccSpecPow);
                ccSpec = clamp(ccSpec, 0.0, 5.0); // DEFENSIVE: Clamp result
        
                color += vec3(0.25) * clearcoat * ccSpec;
            }

            // --- Sheen ---
            if (length(sheenColor) > 0.001) {
                float NdotV = max(dot(N, V), 0.0);
                float sheen = pow(clamp(1.0 - NdotV, 0.0, 1.0), 2.0);
                sheen = clamp(sheen, 0.0, 1.0); // DEFENSIVE: Clamp sheen
                color += sheenColor * sheen * 0.5;
            }

            // --- Transmission (simplified glassy look) ---
            if (transmission > 0.001) {
                vec3 glassTint = mix(albedo, vec3(1.0),
                                     clamp((ior - 1.0) * 0.3, 0.0, 1.0));
                color = mix(color, glassTint, transmission);
            }

           // --- Rim lighting (edge glow) ---
            // View and normal are already available (V, N)
            float rim = 1.0 - max(dot(N, V), 0.0);
            rim = pow(clamp(rim, 0.0, 1.0), 2.0);                // Sharpen falloff
            rim = clamp(rim, 0.0, 1.0); // DEFENSIVE: Clamp rim
    
            //vec3 rimColor = vec3(0.25, 0.25, 0.3) * rim * 0.6; // cool subtle tint
            vec3 rimColor = albedo * rim * 0.2; // or the material's albedo
            color += rimColor;
    
            // Clamp color before brightness boost to prevent overflow
            color = clamp(color, vec3(0.0), vec3(10.0));
    
            // --- Final brightness boost ---
            float brightnessMult = mix(1.1, 2.5, metalness);
            color *= brightnessMult;
    
            // Final color clamping before edge calculation
            color = clamp(color, vec3(0.0), vec3(20.0));
    
            float edge = max(dot(N, V), 0.0);
            float smoothEdge = smoothstep(0.0, 0.05, edge); // fade out last 5% near rim
    
            // Ensure smoothEdge is valid
            smoothEdge = clamp(smoothEdge, 0.0, 1.0);
    
            // Final output clamping
            vec3 finalColor = clamp(color, vec3(0.0), vec3(1000.0));
            float finalOpacity = clamp(opacity * smoothEdge, 0.0, 1.0);
    
            FragColor = vec4(finalColor, 1.0) * smoothEdge;
        }
    )");

    shader.link();
    initSphereMesh();
}

void MaterialPreviewWidget::resizeGL(int w, int h)
{
    proj.setToIdentity();
    proj.perspective(45.0f, float(w)/float(h), 0.1f, 10.0f);

    view.setToIdentity();
    view.translate(0,0,-3.0f);
}

void MaterialPreviewWidget::paintGL()
{
    glClearColor(0.15f,0.15f,0.18f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader.bind();

    QMatrix4x4 model;
    model.rotate(25, 1,0,0);
    model.rotate(30, 0,1,0);

    QMatrix4x4 mvp = proj * view * model;
    QMatrix3x3 normalMat = model.normalMatrix();

    shader.setUniformValue("uMVP", mvp);
    shader.setUniformValue("uModel", model);
    shader.setUniformValue("uNormalMatrix", normalMat);
    shader.setUniformValue("uCamPos", QVector3D(0,0,3));

    shader.setUniformValue("uAlbedo", currentMaterial.albedoColor());
    shader.setUniformValue("uMetalness", currentMaterial.metalness());
    shader.setUniformValue("uRoughness", currentMaterial.roughness());
    shader.setUniformValue("uOpacity", currentMaterial.opacity());
    shader.setUniformValue("uClearcoat", currentMaterial.clearcoat());
    shader.setUniformValue("uClearcoatRoughness", currentMaterial.clearcoatRoughness());
    shader.setUniformValue("uSheenColor", currentMaterial.sheenColor());
    shader.setUniformValue("uSheenRoughness", currentMaterial.sheenRoughness());
    shader.setUniformValue("uTransmission", currentMaterial.transmission());
    shader.setUniformValue("uIOR", currentMaterial.ior());
    shader.setUniformValue("uSpecular", currentMaterial.specular());

	// Set up simple lighting
    shader.setUniformValue("uLights[0].position", QVector3D(3.0f, 3.0f, 3.0f));
    shader.setUniformValue("uLights[0].color", QVector3D(1.0f, 1.0f, 1.0f));

    shader.setUniformValue("uLights[1].position", QVector3D(-3.0f, 3.0f, 1.0f));
    shader.setUniformValue("uLights[1].color", QVector3D(0.8f, 0.8f, 0.8f));

    shader.setUniformValue("uLights[2].position", QVector3D(0.0f, -3.0f, 2.0f));
    shader.setUniformValue("uLights[2].color", QVector3D(0.6f, 0.6f, 0.6f));


    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    shader.release();
}

void MaterialPreviewWidget::initSphereMesh()
{
    const int X_SEGMENTS = 64;
    const int Y_SEGMENTS = 64;

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    for (int y = 0; y <= Y_SEGMENTS; ++y) {
        for (int x = 0; x <= X_SEGMENTS; ++x) {
            float xSeg = (float)x / (float)X_SEGMENTS;
            float ySeg = (float)y / (float)Y_SEGMENTS;
            float xPos = std::cos(xSeg * 2.0f * M_PI) * std::sin(ySeg * M_PI);
            float yPos = std::cos(ySeg * M_PI);
            float zPos = std::sin(xSeg * 2.0f * M_PI) * std::sin(ySeg * M_PI);

            vertices.push_back(xPos);
            vertices.push_back(yPos);
            vertices.push_back(zPos);
            vertices.push_back(xPos);
            vertices.push_back(yPos);
            vertices.push_back(zPos);
        }
    }

    for (int y = 0; y < Y_SEGMENTS; ++y) {
        for (int x = 0; x < X_SEGMENTS; ++x) {
            int i0 = y * (X_SEGMENTS+1) + x;
            int i1 = i0 + X_SEGMENTS + 1;
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i0+1);

            indices.push_back(i0+1);
            indices.push_back(i1);
            indices.push_back(i1+1);
        }
    }

    indexCount = (int)indices.size();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

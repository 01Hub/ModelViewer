#version 450 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
uniform float near_plane;
uniform float far_plane;

uniform vec2 u_screenSize;
uniform sampler2D transmissionColorTexture;
uniform sampler2D transmissionDepthTexture;

// required when using a perspective projection matrix
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

void main()
{
    /*float depthValue = texture(depthMap, TexCoords).r;
    // FragColor = vec4(vec3(LinearizeDepth(depthValue) / far_plane), 1.0); // perspective
    FragColor = vec4(vec3(depthValue), 1.0); // orthographic*/

    vec2 screenUV = gl_FragCoord.xy / vec2(u_screenSize.x, u_screenSize.y);    
    // Just output what's captured in the transmission texture
    vec3 capturedScene = texture(transmissionColorTexture, screenUV).rgb;
    FragColor = vec4(capturedScene, 1.0);
	
	float depth = texture(transmissionDepthTexture, screenUV).r;
    //FragColor = vec4(vec3(1.0 - depth), 1.0);	
}
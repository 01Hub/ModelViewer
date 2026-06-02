#version 450 core

in vec3 v_position;

uniform vec4 topColor;
uniform vec4 botColor;
uniform vec3 screenCenter;
uniform float floorSize;
uniform float groundReferenceSize;
uniform float opacity;

out vec4 fragColor;

float gridLineMask(vec2 planePos, float cellSize, float lineWidthPixels)
{
	vec2 scaled = planePos / max(cellSize, 1e-4);
	vec2 derivative = max(fwidth(scaled), vec2(1e-4));
	vec2 grid = abs(fract(scaled - 0.5) - 0.5) / derivative;
	float line = min(grid.x, grid.y);
	return 1.0 - clamp(line / max(lineWidthPixels, 1e-4), 0.0, 1.0);
}

void main()
{
	vec2 planePos = v_position.xy - screenCenter.xy;
	float sceneScale = max(groundReferenceSize, 1.0);
	float majorCell = max(sceneScale / 12.0, 0.25);
	float minorCell = majorCell / 10.0;

	float majorMask = gridLineMask(planePos, majorCell, 1.55);
	float minorMask = gridLineMask(planePos, minorCell, 1.20);

	float radialGridDistance = length(planePos);
	float farBlend = smoothstep(sceneScale * 0.18, sceneScale * 0.82, radialGridDistance);
	float minorVisibility = max(1.0 - farBlend, 0.22);

	float majorAlpha = majorMask * 0.92;
	float minorAlpha = minorMask * minorVisibility * 0.62;
	float axisXAlpha = gridLineMask(vec2(planePos.x, 0.0), majorCell, 2.0) * 0.55;
	float axisYAlpha = gridLineMask(vec2(0.0, planePos.y), majorCell, 2.0) * 0.55;

	vec3 baseColor = vec3(0.94);
	baseColor = mix(baseColor, vec3(0.35), clamp(minorAlpha, 0.0, 1.0));
	baseColor = mix(baseColor, vec3(0.0), clamp(majorAlpha, 0.0, 1.0));
	baseColor = mix(baseColor, vec3(0.92, 0.48, 0.40), clamp(axisXAlpha, 0.0, 1.0));
	baseColor = mix(baseColor, vec3(0.40, 0.64, 0.92), clamp(axisYAlpha, 0.0, 1.0));

	float alpha = max(majorAlpha, minorAlpha);
	alpha = max(alpha, axisXAlpha);
	alpha = max(alpha, axisYAlpha);
	alpha *= 0.95;

	float floorRadius = floorSize * 0.5;
	float fadeStart = floorRadius * 0.65;
	float fadeEnd = floorRadius * 1.025;
	float distance = radialGridDistance;
	if (distance > fadeEnd)
		discard;

	float fadeFactor = smoothstep(fadeStart, fadeEnd, distance);
	if (fadeFactor >= 1.0)
		discard;

	fragColor = vec4(baseColor, alpha * (1.0 - fadeFactor) * opacity);
}

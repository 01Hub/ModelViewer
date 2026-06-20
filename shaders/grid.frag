#version 450 core

in vec2 v_uv;

uniform mat4 inverseViewProjectionMatrix;
uniform mat4 viewProjectionMatrix;
uniform vec3 cameraPos;
uniform vec3 screenCenter;
uniform float floorSize;
uniform float groundReferenceSize;
uniform float gridPlaneZ;
uniform int worldUpAxis = 2;
uniform float opacity;

out vec4 fragColor;

vec3 unprojectPoint(float ndcZ)
{
	vec4 clip = vec4(v_uv * 2.0 - 1.0, ndcZ, 1.0);
	vec4 world = inverseViewProjectionMatrix * clip;
	return world.xyz / world.w;
}

float gridLineMask(vec2 planePos, float cellSize, float lineWidthPixels)
{
	vec2 scaled = planePos / max(cellSize, 1e-4);
	vec2 derivative = max(fwidth(scaled), vec2(1e-4));
	vec2 grid = abs(fract(scaled - 0.5) - 0.5) / derivative;
	float line = min(grid.x, grid.y);
	return 1.0 - clamp(line / max(lineWidthPixels, 1e-4), 0.0, 1.0);
}

float satf(float x)
{
	return clamp(x, 0.0, 1.0);
}

vec2 satv(vec2 x)
{
	return clamp(x, vec2(0.0), vec2(1.0));
}

float max2(vec2 v)
{
	return max(v.x, v.y);
}

float log10f(float x)
{
	return log(x) / log(10.0);
}

void main()
{
	vec3 nearPoint = unprojectPoint(-1.0);
	vec3 farPoint = unprojectPoint(1.0);

	float nearUp = (worldUpAxis == 1) ? nearPoint.y : nearPoint.z;
	float farUp = (worldUpAxis == 1) ? farPoint.y : farPoint.z;
	float denom = farUp - nearUp;
	if (abs(denom) < 1e-6)
		discard;

	float t = (gridPlaneZ - nearUp) / denom;
	if (t < 0.0)
		discard;

	vec3 hitPoint = nearPoint + t * (farPoint - nearPoint);
	vec4 clipPos = viewProjectionMatrix * vec4(hitPoint, 1.0);
	float depth = clipPos.z / clipPos.w;
	gl_FragDepth = depth * 0.5 + 0.5;
	if (gl_FragDepth < 0.0 || gl_FragDepth > 1.0)
		discard;

	vec2 planePos = (worldUpAxis == 1)
		? (hitPoint.xz - screenCenter.xz)
		: (hitPoint.xy - screenCenter.xy);
	float radialGridDistance = length(planePos);
	float sceneScale = max(groundReferenceSize, 1.0);
	float gridCellSize = max(sceneScale / 120.0, 0.01);
	float minPixelsBetweenCells = 2.0;

	vec2 dvx = vec2(dFdx(planePos.x), dFdy(planePos.x));
	vec2 dvy = vec2(dFdx(planePos.y), dFdy(planePos.y));
	vec2 dudv = vec2(length(dvx), length(dvy));
	float l = length(dudv);
	float lod = max(0.0, log10f(l * minPixelsBetweenCells / gridCellSize) + 1.0);

	float cellLod0 = gridCellSize * pow(10.0, floor(lod));
	float cellLod1 = cellLod0 * 10.0;
	float cellLod2 = cellLod1 * 10.0;

	vec2 aaDudv = max(dudv * 5.25, vec2(1e-5));

	vec2 modDiv = mod(planePos, cellLod0) / aaDudv;
	float lod0a = max2(vec2(1.0) - abs(satv(modDiv) * 2.0 - vec2(1.0)));

	modDiv = mod(planePos, cellLod1) / aaDudv;
	float lod1a = max2(vec2(1.0) - abs(satv(modDiv) * 2.0 - vec2(1.0)));

	modDiv = mod(planePos, cellLod2) / aaDudv;
	float lod2a = max2(vec2(1.0) - abs(satv(modDiv) * 2.0 - vec2(1.0)));

	float lodFade = fract(lod);

	float axisXAlpha = gridLineMask(vec2(planePos.x, 0.0), cellLod2, 2.8) * 0.6;
	float axisYAlpha = gridLineMask(vec2(0.0, planePos.y), cellLod2, 2.8) * 0.6;

	vec3 thinColor = vec3(0.45);
	vec3 thickColor = vec3(0.18);
	vec3 baseColor = vec3(0.82);
	float alpha = 0.0;

	if (lod2a > 0.0)
	{
		baseColor = thickColor;
		alpha = lod2a;
	}
	else if (lod1a > 0.0)
	{
		baseColor = mix(thickColor, thinColor, lodFade);
		alpha = lod1a;
	}
	else
	{
		baseColor = thinColor;
		alpha = lod0a * (1.0 - lodFade);
	}

	baseColor = mix(baseColor, vec3(0.92, 0.48, 0.40), clamp(axisXAlpha, 0.0, 1.0));
	baseColor = mix(baseColor, vec3(0.40, 0.64, 0.92), clamp(axisYAlpha, 0.0, 1.0));
	alpha = max(alpha, axisXAlpha);
	alpha = max(alpha, axisYAlpha);
	alpha *= 0.95;

	float floorRadius = floorSize * 0.5;
	float fadeStart = floorRadius * 0.65;
	float fadeEnd = floorRadius * 1.025;
	if (radialGridDistance > fadeEnd)
		discard;

	float fadeFactor = smoothstep(fadeStart, fadeEnd, radialGridDistance);
	if (fadeFactor >= 1.0)
		discard;

	fragColor = vec4(baseColor, alpha * (1.0 - fadeFactor) * opacity);
}

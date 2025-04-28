#version 460

void main() {
	float depth = gl_FragCoord.z / gl_FragCoord.w;
	float normalizedDepth = (depth + 1.0) * 0.5;
	gl_FragDepth = normalizedDepth;
}
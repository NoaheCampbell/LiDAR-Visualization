#version 330 core
in vec4 vertexColor;
in float vertexAlpha;
in float pointDistance;

out vec4 FragColor;

void main() {
    // Calculate distance from center of point
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distance = length(coord);
    
    // Create circular points with smooth edges
    if (distance > 0.5) {
        discard;
    }
    
    // Calculate alpha with smooth anti-aliasing
    float alpha = 1.0 - smoothstep(0.35, 0.5, distance);
    
    // Apply vertex alpha (for fading)
    alpha *= vertexAlpha;
    
    // Add subtle depth-based fading for far points
    float depthFade = 1.0;
    if (pointDistance > 100.0) {
        depthFade = clamp(1.0 - (pointDistance - 100.0) / 200.0, 0.3, 1.0);
    }
    alpha *= depthFade;
    
    // Add subtle center highlight for better visibility
    float centerHighlight = 1.0 - distance * 0.3;
    
    FragColor = vec4(vertexColor.rgb * centerHighlight, alpha);
}
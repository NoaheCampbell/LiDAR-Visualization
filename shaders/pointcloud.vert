#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in float aIntensity;
layout (location = 2) in vec4 aColor;
layout (location = 3) in float aSize;
layout (location = 4) in float aTimestamp;
layout (location = 5) in int aRoverId;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPosition;
uniform float pointSizeMultiplier;
uniform bool enableSizeAttenuation;
uniform float currentTime;
uniform float fadeTime;
uniform int renderMode;

out vec4 vertexColor;
out float vertexAlpha;
out float pointDistance;

// Color mapping functions
vec4 getRoverColor(int roverId) {
    if (roverId == 1) return vec4(1.0, 0.2, 0.2, 1.0);      // Red
    else if (roverId == 2) return vec4(0.2, 1.0, 0.2, 1.0); // Green
    else if (roverId == 3) return vec4(0.2, 0.2, 1.0, 1.0); // Blue
    else if (roverId == 4) return vec4(1.0, 1.0, 0.2, 1.0); // Yellow
    else if (roverId == 5) return vec4(1.0, 0.2, 1.0, 1.0); // Magenta
    else {
        // Generate color for rovers > 5 using simple hash
        float hue = float(roverId * 137) / 360.0; // Golden angle approximation
        hue = fract(hue);
        
        // Simple HSV to RGB conversion
        float c = 1.0; // Full saturation
        float x = 1.0 - abs(mod(hue * 6.0, 2.0) - 1.0);
        
        if (hue < 1.0/6.0) return vec4(c, x, 0, 1.0);
        else if (hue < 2.0/6.0) return vec4(x, c, 0, 1.0);
        else if (hue < 3.0/6.0) return vec4(0, c, x, 1.0);
        else if (hue < 4.0/6.0) return vec4(0, x, c, 1.0);
        else if (hue < 5.0/6.0) return vec4(x, 0, c, 1.0);
        else return vec4(c, 0, x, 1.0);
    }
}

vec4 getHeightColor(float height) {
    // Map height to blue-green-red gradient
    float normalizedHeight = clamp((height + 10.0) / 20.0, 0.0, 1.0);
    
    if (normalizedHeight < 0.5) {
        // Blue to Green
        float t = normalizedHeight * 2.0;
        return vec4(0.0, t, 1.0 - t, 1.0);
    } else {
        // Green to Red
        float t = (normalizedHeight - 0.5) * 2.0;
        return vec4(t, 1.0 - t, 0.0, 1.0);
    }
}

vec4 getIntensityColor(float intensity) {
    // Grayscale based on intensity
    return vec4(intensity, intensity, intensity, 1.0);
}

vec4 getTimestampColor(float timestamp, float currentTime) {
    // Color based on age: recent = hot colors, old = cool colors
    float age = currentTime - timestamp;
    float normalizedAge = clamp(age / 30.0, 0.0, 1.0);
    
    // Hot to cool: Red -> Yellow -> Green -> Blue
    if (normalizedAge < 0.33) {
        float t = normalizedAge / 0.33;
        return vec4(1.0, t, 0.0, 1.0); // Red to Yellow
    } else if (normalizedAge < 0.66) {
        float t = (normalizedAge - 0.33) / 0.33;
        return vec4(1.0 - t, 1.0, 0.0, 1.0); // Yellow to Green
    } else {
        float t = (normalizedAge - 0.66) / 0.34;
        return vec4(0.0, 1.0 - t, t, 1.0); // Green to Blue
    }
}

void main() {
    gl_Position = projection * view * vec4(aPosition, 1.0);
    
    // Calculate distance to camera for attenuation
    pointDistance = length(cameraPosition - aPosition);
    
    // Calculate point color based on render mode
    if (renderMode == 0) {          // SOLID
        vertexColor = aColor;
    } else if (renderMode == 1) {   // INTENSITY
        vertexColor = getIntensityColor(aIntensity);
    } else if (renderMode == 2) {   // HEIGHT
        vertexColor = getHeightColor(aPosition.z);
    } else if (renderMode == 3) {   // ROVER_ID
        vertexColor = getRoverColor(aRoverId);
    } else if (renderMode == 4) {   // TIMESTAMP
        vertexColor = getTimestampColor(aTimestamp, currentTime);
    } else {
        vertexColor = aColor;
    }
    
    // Calculate fade alpha based on timestamp
    if (fadeTime > 0.0) {
        float age = currentTime - aTimestamp;
        vertexAlpha = 1.0 - clamp(age / fadeTime, 0.0, 1.0);
        vertexAlpha = max(vertexAlpha, 0.1); // Minimum alpha for visibility
    } else {
        vertexAlpha = 1.0;
    }
    
    // Calculate point size with distance attenuation
    float pointSize = aSize * pointSizeMultiplier;
    
    if (enableSizeAttenuation) {
        // Quadratic attenuation with configurable parameters
        float constant = 1.0;
        float linear = 0.045;
        float quadratic = 0.0075;
        
        float attenuation = 1.0 / (constant + linear * pointDistance + quadratic * pointDistance * pointDistance);
        pointSize = pointSize * attenuation * 100.0; // Scale factor to make points visible
        
        // Clamp point size to reasonable range
        pointSize = clamp(pointSize, 1.0, 16.0);
    }
    
    // Apply additional size scaling based on distance
    float distanceScale = 1.0;
    if (pointDistance > 50.0) {
        distanceScale = 50.0 / pointDistance;
        distanceScale = clamp(distanceScale, 0.2, 1.0);
    }
    
    gl_PointSize = pointSize * distanceScale;
}
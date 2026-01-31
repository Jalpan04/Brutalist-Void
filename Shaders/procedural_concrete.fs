#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Output fragment color
out vec4 finalColor;

// Uniforms
uniform vec3 viewPos;
uniform vec3 lightDir;      // Direction *to* the light (or from light, depending on convention. Usually Direction TO light for dot prod)
// Raylib standard for lightDir in default shader suggests "direction from light" or "direction to light"? 
// Usually Raylib sends Light position/direction. We'll use a custom one.
uniform vec4 lightColor;
uniform vec4 ambientColor;
uniform float time;
uniform int creepyMode; // 1 = creepy/dark, 0 = liminal/atmospheric

// Custom function for noise
float hash(float n) { return fract(sin(n) * 43758.5453123); }
float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0 + 113.0 * p.z;
    return mix(mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
                   mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
               mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                   mix(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
}

void main()
{
    // 1. Basic properties
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(-lightDir); 
    float diff = max(dot(normal, lightDirection), 0.0);
    
    // 2. Formwork Lines (Horizontal wood patterns)
    float formwork = sin(fragPosition.y * 5.0 + noise(fragPosition * 2.0)); 
    formwork = smoothstep(-0.95, 0.0, formwork); // Sharp grooves
    
    // 3. Concrete Grit/Stains
    float grain = noise(fragPosition * 30.0); // Fine grain
    float stain = noise(fragPosition * 0.5 + vec3(0, time * 0.01, 0)); // Large vertical stains
    
    // 4. "Edge Darkening" (Cheap AO)
    // We use fract of world position. Since cubes are likely integer aligned or scaled, 
    // the edges of the 1.0 unit cubes (if scaled) might align with fract ~ 0 or 1.
    // However, pillars are scaled. We can try using the normal to guess which axis to check.
    // A simple trick: darker near the integer boundaries of the world position * scale.
    // Let's assume standard UVs might be useful, but we only have world pos.
    // We'll use a high-frequency grid pattern to fake "blocks" or "seams".
    vec3 grid = abs(fract(fragPosition * 0.5 - 0.5) - 0.5);
    float edge = max(max(grid.x, grid.y), grid.z);
    edge = smoothstep(0.48, 0.5, edge); // 1.0 at edge, 0.0 center
    
    // Base Color (Concrete Grey)
    vec3 baseCol = vec3(0.45, 0.46, 0.48);
    // Darker in formwork grooves, noisy pores, and edges
    vec3 albedo = baseCol;
    albedo *= (0.8 + 0.2 * noise(fragPosition * 4.0)); // General variation
    albedo *= (0.5 + 0.5 * formwork); // Dark lines
    albedo *= (1.0 - edge * 0.6); // Dark Edges
    albedo *= (0.6 + 0.4 * stain); // Stains

    // 5. Lighting Parameters branching
    vec3 result;
    vec3 fogColor;
    float fogCoeff;

    // View direction for specular
    // View direction for specular (Using pre-calculated lightDirection)
    vec3 viewDir = normalize(viewPos - fragPosition);
    // lightDirection already calculated above
    vec3 halfDir = normalize(lightDirection + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), 32.0); // Sharp specular

    if (creepyMode == 1) {
        // --- CREEPY MODE ---
        // "Moonlight" Ambience - Blueish grey, not black
        vec3 ambient = ambientColor.rgb * vec3(0.1, 0.12, 0.15); 
        vec3 diffuse = lightColor.rgb * diff * 0.6;
        vec3 specular = vec3(0.2) * spec; // Subtle wet dampness
        
        vec3 lighting = ambient + diffuse + specular;
        result = albedo * lighting;

        // Window Emissive (Vertical only, spatially separated)
        float verticalness = abs(normal.y);
        if (verticalness < 0.3) {
            float windowHash = hash(floor(fragPosition.x * 0.2) + floor(fragPosition.y * 0.3) * 57.0 + floor(fragPosition.z * 0.2) * 113.0);
            float n1 = hash(floor(fragPosition.x * 0.2 + 1.0) + floor(fragPosition.y * 0.3) * 57.0 + floor(fragPosition.z * 0.2) * 113.0);
            float n2 = hash(floor(fragPosition.x * 0.2) + floor(fragPosition.y * 0.3 + 1.0) * 57.0 + floor(fragPosition.z * 0.2) * 113.0);
            float n3 = hash(floor(fragPosition.x * 0.2) + floor(fragPosition.y * 0.3) * 57.0 + floor(fragPosition.z * 0.2 + 1.0) * 113.0);
            
            if (windowHash > 0.92 && n1 < 0.92 && n2 < 0.92 && n3 < 0.92) {
                // Dimmer, more "lived in" orange
                vec3 lightGlow = vec3(1.0, 0.6, 0.3) * 1.5;
                float flicker = 0.8 + 0.2 * sin(time * 3.0 + fragPosition.x);
                result = mix(result, lightGlow * flicker, 0.7);
            }
        }

        // --- NEW: PROCEDURAL GRAFFITI ---
        // Strange symbols on lower walls
        if (verticalness < 0.1 && fragPosition.y < 6.0 && fragPosition.y > 1.0) {
            float graffitiNoise = noise(fragPosition * 2.0); // Placement noise
            if (graffitiNoise > 0.6) {
                // Generate primitive "glyphs" using high freq domain warping
                vec3 gPos = fragPosition * 8.0;
                float glyph = hash(floor(gPos.x) + floor(gPos.y) * 57.0);
                float glyphShape = step(0.5, fract(glyph * 10.0)); // Simple on/off blocks
                
                if (glyph > 0.8) {
                    vec3 neonColor = vec3(0.9, 0.1, 0.1); // Red Neon
                    float pulse = 0.8 + 0.2 * sin(time * 2.0 + fragPosition.x);
                    result = mix(result, neonColor * pulse, 0.5); 
                }
            }
        }

        // --- NEW: BLACK LIQUID POOLS ---
        // On flat floors (normal up) and low height
        if (normal.y > 0.9 && fragPosition.y < 1.0) {
            float puddleNoise = noise(fragPosition * 0.8 + vec3(time*0.05, 0, 0)); // Slow moving liquid potential
            puddleNoise = smoothstep(0.4, 0.45, puddleNoise); // Sharpen edges
            
            if (puddleNoise > 0.5) {
                // It is a puddle
                vec3 liquidColor = vec3(0.02, 0.02, 0.02); // Almost black
                float liquidSpec = pow(max(dot(normal, halfDir), 0.0), 128.0); // Very sharp wet reflection
                
                // Mix: mostly liquid color + fierce specular reflection + mirror-world fake reflection?
                // Just strong specular usually sells "wet" well enough in simple shaders
                result = liquidColor + vec3(liquidSpec * 2.0); 
            }
        }

        
        fogColor = vec3(0.05, 0.05, 0.06); // Deep blue-grey fog
        fogCoeff = 0.02; // Very Dense Fog (Visibility dropoff fast)
    } else {
        // --- LIMINAL MODE ---
        // "Overcast Day" Ambience - Flat, bright greys
        vec3 ambient = ambientColor.rgb * vec3(0.25, 0.25, 0.25); // Much brighter ambient
        vec3 diffuse = lightColor.rgb * diff * 0.8; 
        vec3 specular = vec3(0.05) * spec; // Dry concrete, very dull
        
        vec3 lighting = ambient + diffuse + specular;
        result = albedo * lighting;

        fogColor = vec3(0.2, 0.2, 0.2); // Light grey mist
        fogCoeff = 0.008; // Atmospheric Haze
    }
    
    // 6. Final Compositing with Fog
    float fogDistance = length(viewPos - fragPosition);
    float fogFactor = 1.0 / exp(fogDistance * fogCoeff);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    finalColor = vec4(mix(fogColor, result, fogFactor), 1.0);
}

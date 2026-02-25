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
    vec3 lightDirection = normalize(-lightDir); // Assuming uniform is "direction of light rays" (downwards)
    float diff = max(dot(normal, lightDirection), 0.0);
    
    // 2. Formwork Lines (Horizontal wood patterns)
    // Scale y by 10.0 for board frequency
    float formwork = sin(fragPosition.y * 10.0 + noise(fragPosition * 2.0) * 0.5); 
    // Threshold to make it look like grooves
    formwork = smoothstep(-0.2, 0.2, formwork);
    
    // 3. Concrete Grit/Stains
    float grain = noise(fragPosition * 15.0);
    float largeStains = noise(fragPosition * 1.5);
    
    // Base Color (Concrete Grey)
    vec3 baseCol = vec3(0.5, 0.5, 0.55);
    // Darker in the grooves (formwork < 0 effectively) and noisy
    vec3 albedo = baseCol * (0.9 + 0.1 * formwork) * (0.8 + 0.4 * grain) * (0.7 + 0.6 * largeStains);
    
    // 4. Lighting + Flicker
    // "Dying bulb" flicker
    float flicker = 1.0 + 0.05 * sin(time * 15.0) * cos(time * 53.0);
    
    vec3 ambient = ambientColor.rgb * 0.2 * flicker; // Low ambient
    vec3 diffuse = lightColor.rgb * diff * 3.5; // High intensity direct light for Chiaroscuro
    
    vec3 lighting = ambient + diffuse;
    vec3 result = albedo * lighting;
    
    // 5. Fog (Charcoal Grey #1a1a1a)
    vec3 fogColor = vec3(0.102, 0.102, 0.102); // #1a1a1a
    float dist = length(viewPos - fragPosition);
    // Fog starts at 20 units, linear fade to 120
    float fogFactor = clamp((dist - 20.0) / 100.0, 0.0, 1.0);
    
    finalColor = vec4(mix(result, fogColor, fogFactor), 1.0);
}

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

#ifdef TEXTURED_MESH

layout(binding = 0, std140) uniform GlobalParams
{
	vec3 uCameraPosition;
	vec3 uResolution;
};

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(binding = 1, std140) uniform LocalParams
{
	mat4 uWorldMatrix;
	mat4 uWorldViewProjectionMatrix;
};

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out vec2 vTexCoord;
out vec3 vPosition;
out vec3 vNormal;
out vec3 vViewDir;

void main()
{
	vTexCoord = aTexCoord;

	vPosition = vec3(uWorldMatrix * vec4(aPosition, 1.0));
	vNormal = normalize(vec3(uWorldMatrix * vec4(aNormal, 0.0)));
	vViewDir = normalize(uCameraPosition - vPosition);
	gl_Position = uWorldViewProjectionMatrix * vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;
in vec3 vPosition;
in vec3 vNormal;
in vec3 vViewDir;

uniform sampler2D uAlbedo;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oPosition;
layout(location = 3) out vec4 oDepth;

float near = 0.1; 
float far  = 100.0; 
  
float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));	
}    

void main()
{
	oNormal = vec4(vNormal, 1.0);
	oPosition = vec4(vPosition, 1.0);

	float depth = LinearizeDepth(gl_FragCoord.z) / far;
	oDepth = vec4(vec3(depth), 1.0);

	oAlbedo = texture(uAlbedo, vTexCoord);
}

#endif
#endif

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

#ifdef DIRECTIONAL_LIGHT

layout(binding = 0, std140) uniform GlobalParams
{
	vec3 uCameraPosition;
	vec3 uResolution;
};

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location = 0) in vec3 aPosition;
layout(location = 2) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
	vTexCoord = aTexCoord;

	gl_Position = vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

layout(binding = 1, std140) uniform LocalParams
{
	vec3 uColor;
	vec3 uDirection;
};

in vec2 vTexCoord;

uniform sampler2D uAlbedo;
uniform sampler2D uNormals;
uniform sampler2D uPosition;
uniform sampler2D uDepth;
uniform sampler2D uTrueDepth;

layout(location = 0) out vec4 oColor;

void main()
{
	vec3 albedo = texture(uAlbedo, vTexCoord).xyz;
	vec3 normal = texture(uNormals, vTexCoord).xyz;
	vec3 diffuse = uColor * mix(vec3(0), albedo, dot(normal, uDirection)) * 0.7;

	oColor = vec4(diffuse, 1.0);
}

#endif
#endif

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

#ifdef POINT_LIGHT

layout(binding = 0, std140) uniform GlobalParams
{
	vec3 uCameraPosition;
	vec3 uResolution;
};

layout(binding = 1, std140) uniform LocalParams
{
	vec3 uColor;
	vec3 uCenter;
	float uRange;

	mat4 uWorldMatrix;
	mat4 uWorldViewProjectionMatrix;
};

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out vec3 vPosition;

void main()
{
	vPosition = vec3(uWorldMatrix * vec4(aPosition, 1.0));
	gl_Position = uWorldViewProjectionMatrix * vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

uniform sampler2D uAlbedo;
uniform sampler2D uNormals;
uniform sampler2D uPosition;
uniform sampler2D uDepth;

layout(location = 0) out vec4 oColor;

in vec3 vPosition;

float near = 0.1; 
float far  = 100.0; 
  
float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));	
}  

void main()
{
	vec2 texcoord = vec2(gl_FragCoord.x / uResolution.x, gl_FragCoord.y / uResolution.y);
	vec3 albedo = texture(uAlbedo, texcoord).xyz;
	vec3 normal = texture(uNormals, texcoord).xyz;
	vec3 position = texture(uPosition, texcoord).xyz;

	float depth = texture(uDepth, texcoord).x;
	float vDepth = LinearizeDepth(gl_FragCoord.z) / far;

	vec3 direction = uCenter - position;
	float dist = length(direction);
	if (vDepth < depth && dist < uRange && depth > 0.0)
	{
		direction = normalize(direction);
		vec3 diffuse = uColor * (mix(vec3(0), albedo, dot(normal, direction)) * 0.7) * (1 - dist / uRange);

		oColor = vec4(diffuse,1);
	}
	else
		oColor = vec4(0,0,0,0);
}

#endif
#endif

// NOTE: You can write several shaders in the same file if you want as
// long as you embrace them within an #ifdef block (as you can see above).
// The third parameter of the LoadProgram function in engine.cpp allows
// chosing the shader you want to load by name.

/* LIGHT STUFF

struct Light
{
    unsigned int type;
    vec3 color;
	vec3 direction;
    vec3 position;
};

layout(binding = 0, std140) uniform GlobalParams
{
	vec3 uCameraPosition;
	unsigned int uLightCount;
	Light uLight[16];

};

	vec3 color = vec3(0);
	for (unsigned int i = 0; i < uLightCount; ++i)
		switch (uLight[i].type)
		{
			case 0: // DIRECTIONAL
				vec3 diffuse = uLight[i].color * mix(vec3(0), textureColor, dot(vNormal, uLight[i].direction)) * 0.7;
	
				vec3 ambiental = uLight[i].color * 0.2;
	
				vec3 specVec = normalize(reflect(uLight[i].direction, vNormal));
				float spec = -dot(specVec, vViewDir);
				spec = clamp(spec, 0.0, 1.0);
				spec = pow(spec, 64.0);
				vec3 specular = uLight[i].color * spec;
	
				color += diffuse + ambiental + specular;
				break;
	
			case 1: // POINT
				break;
		}
	color /= uLightCount;
*/
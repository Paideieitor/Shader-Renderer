///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

#ifdef TEXTURED_MESH

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

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(binding = 1, std140) uniform LocalParams
{
	mat4 uWorldMatrix;
	mat4 uWorldViewProjectionMatrix;
};

out vec2 vTexCoord;
out vec3 vPosition;
out vec3 vNormal;
out vec3 vViewDir;

void main()
{
	vTexCoord = aTexCoord;

	vPosition = vec3(uWorldMatrix * vec4(aPosition, 1.0));
	vNormal = vec3(uWorldMatrix * vec4(aNormal, 0.0));
	vViewDir = normalize(uCameraPosition - vPosition);
	gl_Position = uWorldViewProjectionMatrix * vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;
in vec3 vPosition;
in vec3 vNormal;
in vec3 vViewDir;

uniform sampler2D uTexture;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oAlbedo;
layout(location = 2) out vec4 oNormal;
layout(location = 3) out vec4 oPosition;
layout(location = 4) out vec4 oDepth;

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

	vec3 textureColor = texture(uTexture, vTexCoord).xyz;
	oAlbedo = vec4(textureColor, 1.0);

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
	
	oColor = vec4(color, 1.0);
}

#endif
#endif

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

#ifdef TEXTURE_TO_SCREEN

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

void main()
{
	vTexCoord = aTexCoord;
	gl_Position = vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;

uniform sampler2D uTexture;

layout(location = 0) out vec4 oColor;

void main()
{
	oColor = texture(uTexture, vTexCoord);
}

#endif
#endif

// NOTE: You can write several shaders in the same file if you want as
// long as you embrace them within an #ifdef block (as you can see above).
// The third parameter of the LoadProgram function in engine.cpp allows
// chosing the shader you want to load by name.

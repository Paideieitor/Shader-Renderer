///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

#ifdef TEXTURED_MESH

layout(binding = 0, std140) uniform GlobalParams
{
	vec3 uCameraPosition;
	vec3 uResolution;
	float znear;
	float zfar;
};

layout(binding = 1, std140) uniform LocalParams
{
	mat4 uWorldMatrix;
	mat4 uWorldViewProjectionMatrix;

	unsigned int hasNormalMapping;
	unsigned int hasReliefMapping;
};

#if defined(VERTEX) ///////////////////////////////////////////////////

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out vec2 vTexCoord;
out vec3 vPosition;
out vec3 vNormal;
out vec3 vViewDir;
out mat3 vTBN;
out vec3 vTanViewPos;
out vec3 vTanFragPos;

void main()
{
	vTexCoord = aTexCoord;

	vPosition = vec3(uWorldMatrix * vec4(aPosition, 1.0));
	vNormal = normalize(vec3(uWorldMatrix * vec4(aNormal, 0.0)));
	vViewDir = normalize(uCameraPosition - vPosition);

	vec3 T = normalize(vec3(uWorldMatrix * vec4(aTangent, 0.0)));
	vec3 B = normalize(vec3(uWorldMatrix * vec4(aBitangent, 0.0)));

	vTBN = mat3(T, B, vNormal);

	vTanViewPos  = vTBN * uCameraPosition;
    vTanFragPos  = vTBN * vPosition;

	gl_Position = uWorldViewProjectionMatrix * vec4(aPosition, 1.0);
}

#elif defined(FRAGMENT) ///////////////////////////////////////////////

in vec2 vTexCoord;
in vec3 vPosition;
in vec3 vNormal;
in vec3 vViewDir;
in mat3 vTBN;
in vec3 vTanViewPos;
in vec3 vTanFragPos;

uniform sampler2D uAlbedo;
uniform sampler2D uNormal;
uniform sampler2D uRelief;

layout(location = 0) out vec4 oAlbedo;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oPosition;
layout(location = 3) out vec4 oDepth;
  
float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * znear * zfar) / (zfar + znear - z * (zfar - znear)) / zfar;
}    

void main()
{	
	vec3 normal = vNormal;
	vec2 UVs = vTexCoord;
	float depth = gl_FragCoord.z;

	if (hasReliefMapping > 0)
	{
		vec3 tanViewDir = normalize(vTanFragPos - vTanViewPos);
		float heightScale = 0.05;
		const float minLayers = 8.0 * 5.0;
		const float maxLayers = 64.0 * 5.0;
		float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0,0,1), tanViewDir)));
		float layerDepth = 1.0 / numLayers;
		float currentLayerDepth = 0.0;
	
		vec2 S = vec2(tanViewDir.x, tanViewDir.y) / tanViewDir.z * heightScale; 
		vec2 deltaUVs = S / numLayers;
		
		float currentDepthMapValue = 1.0 - texture(uRelief, UVs).r;
		while(currentLayerDepth < currentDepthMapValue)
		{
		    UVs -= deltaUVs;
		    currentDepthMapValue = 1.0 - texture(uRelief, UVs).r;
		    currentLayerDepth += layerDepth;
		}
	
		vec2 prevTexCoords = UVs + deltaUVs;
		float afterDepth  = currentDepthMapValue - currentLayerDepth;
		float beforeDepth = 1.0 - texture(uRelief, prevTexCoords).r - currentLayerDepth + layerDepth;
		float weight = afterDepth / (afterDepth - beforeDepth);
		
		UVs = prevTexCoords * weight + UVs * (1.0f - weight);

		// NO DEPTH
		// IDEA -> USE texture(uRelief, UVs) AND heightScale TO UPDATE THE DEPTH VALUE

		// Get rid of anything outside the normal range
		if(UVs.x > 1.0 || UVs.y > 1.0 || UVs.x < 0.0 || UVs.y < 0.0)
			discard;
	}

	if (hasNormalMapping > 0)
	{
		normal = normalize(texture(uNormal, UVs).xyz);

		normal = normalize(normal * 2.0 - 1.0);
		normal.y *= -1;

		normal = normalize(vTBN * normal);
	}

	oAlbedo = texture(uAlbedo, UVs);
	oNormal = vec4(normal, 1.0);
	oPosition = vec4(vPosition, 1.0);

	gl_FragDepth = depth;
	oDepth = vec4(vec3(LinearizeDepth(depth)), 1.0);
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
	float znear;
	float zfar;
};

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

layout(location = 0) out vec4 oColor;

void main()
{
	vec3 albedo = texture(uAlbedo, vTexCoord).xyz;
	vec3 normal = texture(uNormals, vTexCoord).xyz;
	vec3 position = texture(uPosition, vTexCoord).xyz;
	vec3 viewDir = normalize(uCameraPosition - position);

	vec3 diffuse = uColor * mix(vec3(0), albedo, dot(normal, uDirection)) * 0.7;

	vec3 ambiental = uColor * 0.1;

	vec3 specVec = normalize(reflect(uDirection, normal));
	float spec = -dot(specVec, viewDir);
	spec = clamp(spec, 0.0, 1.0);
	spec = pow(spec, 64.0);
	vec3 specular = uColor * spec;

	oColor = vec4(diffuse + ambiental + specular, 1.0);
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
	float znear;
	float zfar;
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
  
float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * znear * zfar) / (zfar + znear - z * (zfar - znear));	
}  

void main()
{
	vec2 texcoord = vec2(gl_FragCoord.x / uResolution.x, gl_FragCoord.y / uResolution.y);
	vec3 albedo = texture(uAlbedo, texcoord).xyz;
	vec3 normal = texture(uNormals, texcoord).xyz;
	vec3 position = texture(uPosition, texcoord).xyz;

	float depth = texture(uDepth, texcoord).x;
	float vDepth = LinearizeDepth(gl_FragCoord.z) / zfar;

	vec3 viewDir = normalize(uCameraPosition - position);

	vec3 direction = uCenter - position;
	float dist = length(direction);
	direction = normalize(direction);

	if (vDepth > depth && dist < uRange && depth > 0.0)
	{
		vec3 diffuse = uColor * (mix(vec3(0), albedo, dot(normal, direction)) * 0.7);

		vec3 ambiental = uColor * 0.1;

		vec3 specVec = normalize(reflect(direction, normal));
		float spec = -dot(specVec, viewDir);
		spec = clamp(spec, 0.0, 1.0);
		spec = pow(spec, 64.0);
		vec3 specular = uColor * spec;

		oColor = vec4((diffuse + ambiental + specular)  * (1 - dist / uRange),1);
	}
	else
		oColor = vec4(0,0,0,0);
}

#endif
#endif

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

#ifdef TO_SCREEN

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

uniform sampler2D uColor;

layout(location = 0) out vec4 oColor;

void main()
{
	oColor = texture(uColor, vTexCoord);
}

#endif
#endif

// NOTE: You can write several shaders in the same file if you want as
// long as you embrace them within an #ifdef block (as you can see above).
// The third parameter of the LoadProgram function in engine.cpp allows
// chosing the shader you want to load by name.
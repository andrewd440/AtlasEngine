#version 430 core

//#pragma optimize(off)
//#pragma debug(off)

layout ( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec3 vNormal;
layout( location = 2 ) in vec4 vColor;

out VS_OUT 
{
	vec4 Color;
	vec3 Normal;
	vec3 Light;
	vec3 Viewer;
} vs_out;

layout(std140, binding = 0) uniform TransformBlock
{
// Member					Base Align		Aligned Offset		End
	mat4 ModelView;			//		16					0			64
	mat4 Projection;		//		16					64			128
} Transforms;


uniform	vec4 LightPosition = vec4(20, 1000000, 1000000, 1);

void main()
{
	vec4 ViewPosition = Transforms.ModelView * vPosition;

	vs_out.Normal = mat3(Transforms.ModelView) * vNormal;
	vs_out.Light = (Transforms.ModelView * LightPosition).xyz - ViewPosition.xyz;
	vs_out.Viewer = -ViewPosition.xyz;

	gl_Position = Transforms.Projection * ViewPosition;
	vs_out.Color = vColor;
}
#type vertex
#version 430 core
layout(location=0) in vec3 a_Position;
layout(location=1) vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

out vec2 a_TexCoord;

void main()
{
  a_TexCoord = a_TexCoord;
  gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}

#type fragment  
#version 430 core

layout(locaiton = 0) out vec4 color;

in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main()
{
  color = texture(u_Texture, v_TexCoord)
}

#version 330 core\n
uniform vec4 light_position;
in vec4 vertex_position;
out vec4 vs_light_direction;
out vec4 light_pos;
void main() {
   gl_Position = vertex_position;
   vs_light_direction = light_position - gl_Position;
   light_pos = light_position;
}

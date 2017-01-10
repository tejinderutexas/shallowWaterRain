#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <ctime>
#include <cstdlib>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

// OpenGL library includes
#include <GL/glew.h>
#include <GLFW/glfw3.h>

std::ostream& operator<<(std::ostream& os, const glm::vec2& v) {
  os << glm::to_string(v);
  return os;
}

std::ostream& operator<<(std::ostream& os, const glm::vec3& v) {
  os << glm::to_string(v);
  return os;
}

std::ostream& operator<<(std::ostream& os, const glm::vec4& v) {
  os << glm::to_string(v);
  return os;
}

int window_width = 800, window_height = 600;
const std::string window_title = "Rain Boiz";

// VBO and VAO descriptors.

// We have these VBOs available for each VAO.
enum {
  kVertexBuffer,
  kVertAttr1,
  kIndexBuffer,
  kNumVbos
};

// These are our VAOs.
enum {
  kBoxVao,
  kPlaneVao,
  kWaterVao,
  kRainVao,
  kNumVaos
};

GLuint array_objects[kNumVaos];  // This will store the VAO descriptors.
GLuint buffer_objects[kNumVaos][kNumVbos];  // These will store VBO descriptors.

float last_x = 0.0f, last_y = 0.0f, current_x = 0.0f, current_y = 0.0f;
bool drag_state = false;
int current_button = -1;
float camera_distance = 4.0;
float pan_speed = 0.1f;
float roll_speed = 0.1f;
float rotation_speed = 0.05f;
float zoom_speed = 0.1f;
bool fps_mode = true;

glm::vec3 eye = glm::vec3(0, 1.0, camera_distance);
glm::vec3 look = glm::vec3(0, 0, -1);
glm::vec3 up = glm::vec3(0, 1, 0);

const char* vertex_shader =
    "#version 410 core\n"
    "uniform vec4 light_position;"
    "in vec4 vertex_position;"
    "out vec4 vs_light_direction;"
    "void main() {"
    "   gl_Position = vertex_position;"
    "   vs_light_direction = light_position - gl_Position;"
    "}";

const char* water_vertex_shader =
    "#version 410 core\n"
    "uniform vec3 look;"
    "uniform vec4 light_position;"
    "in vec4 vertex_position;"
    "in float height;"
    "out vec4 vs_light_direction;"
    "out vec4 vert_viewdir;"
    "out vec4 vert_color;"
//       for some reason using height before vert_color causes the shader to
//       not work.
//
//       old code:
//       newpos[1] = height - 0.3;
//       vs_light_direction = light_position - gl_Position;
//       vert_color = vertex_color;"
//
//       The following shader code actually works. Still investigating why.
    "void main() {"
    "    vec4 newpos = vertex_position;"
    "    newpos[1] = height - 0.3;"
    "    gl_Position = newpos;"
    "    vs_light_direction = light_position - newpos;"
    "    vert_viewdir = vec4(-look, 1.0);"
    "}";

// Add light stuff later would be cool
const char* point_vertex_shader =
    "#version 410 core\n"
    "in vec4 vertex_position;"
    "void main() {"
    "   gl_Position = vertex_position;"
    "}";

const char* geometry_shader =
    "#version 410 core\n"
    "layout (triangles) in;"
    "layout (triangle_strip, max_vertices = 3) out;"
    "uniform mat4 projection;"
    "uniform mat4 view;"
    "in vec4 vs_light_direction[];"
    "out vec4 normal;"
    "out vec4 light_direction;"
    "out vec4 viewdir;"
    "void main() {"
    "   int n = 0;"
    "   vec3 a = gl_in[0].gl_Position.xyz;"
    "   vec3 b = gl_in[1].gl_Position.xyz;"
    "   vec3 c = gl_in[2].gl_Position.xyz;"
    "   vec3 u = normalize(b - a);"
    "   vec3 v = normalize(c - a);"
    "   normal = normalize(vec4(normalize(cross(u, v)), 0.0));"
    "   for (n = 0; n < gl_in.length(); n++) {"
    "       light_direction = normalize(vs_light_direction[n]);"
    "       gl_Position = projection * view * gl_in[n].gl_Position;"
    "       EmitVertex();"
    "   }"
    "   EndPrimitive();"
    "}";

const char* water_geometry_shader =
    "#version 410 core\n"
    "layout (triangles) in;"
    "layout (triangle_strip, max_vertices = 3) out;"
    "uniform mat4 projection;"
    "uniform mat4 view;"
    "in vec4 vs_light_direction[];"
    "in vec4 vert_color[];"
    "in vec4 vert_viewdir[];"
    "out vec4 normal;"
    "out vec4 light_direction;"
    "out vec4 viewdir;"
    "void main() {"
    "   int n = 0;"
    "   vec3 a = gl_in[0].gl_Position.xyz;"
    "   vec3 b = gl_in[1].gl_Position.xyz;"
    "   vec3 c = gl_in[2].gl_Position.xyz;"
    "   vec3 u = normalize(b - a);"
    "   vec3 v = normalize(c - a);"
    "   normal = normalize(vec4(normalize(cross(u, v)), 0.0));"
    "   for (n = 0; n < gl_in.length(); n++) {"
    "       light_direction = normalize(vs_light_direction[n]);"
    "       gl_Position = projection * view * gl_in[n].gl_Position;"
    "       viewdir = vert_viewdir[n];"
    "       EmitVertex();"
    "   }"
    "   EndPrimitive();"
    "}";

// Later make the rain streak or something. would be kinda cool.
const char* point_geometry_shader =
    "#version 410 core\n"
    "layout (points) in;"
    "layout (line_strip, max_vertices = 2) out;"
    "uniform mat4 projection;"
    "uniform mat4 view;"
    "void main() {"
    "    gl_Position = projection * view * gl_in[0].gl_Position;"
    "    EmitVertex();"
    "    gl_Position = projection * view * (vec4(0, 0.1, 0, 0) + gl_in[0].gl_Position);"
    "    EmitVertex();"
    "    EndPrimitive();"
    "}";

const char* point_fragment_shader =
    "#version 410 core\n"
    "out vec4 fragment_color;"
    "void main() {"
    "    fragment_color = vec4(0.0, 0.4, 1.0, 1.0);"
    "}";

const char* fragment_shader =
    "#version 410 core\n"
    "in vec4 normal;"
    "in vec4 light_direction;"
    "in vec4 world_pos;"
    "uniform vec3 diffuse_color;"
    "out vec4 fragment_color;"
    "void main() {"
    "   vec3 color = diffuse_color;"
    "   float dot_nl = dot(normalize(light_direction), normal);"
    "   dot_nl = clamp(dot_nl, 0.0, 1.0);"
    "   color = clamp(dot_nl * color, 0.0, 1.0);"
    "   fragment_color = vec4(color, 1.0);"
    "}";

const char* water_fragment_shader =
    "#version 410 core\n"
    "in vec4 normal;"
    "in vec4 light_direction;"
    "in vec4 viewdir;"
    "out vec4 fragment_color;"
    "void main() {"
    "   int shininess = 2;"
    "   float float_vr = clamp("
    "       dot(normalize(viewdir), reflect(normalize(-light_direction), normal)),"
    "       0.0, 1.0);"
    "   int n = 1;"
    "   for (n = 1; n < shininess; n++){"
    "       float_vr *= float_vr;"
    "   }"
    "   vec4 specular = vec4(1.0, 1.0, 1.0, 1.0) * float_vr;"
    "   float dot_nl = dot(normalize(light_direction), normal);"
    "   dot_nl = clamp(dot_nl, 0.0, 1.0);"
    "   vec4 color = vec4(0.0, 0.6, 1.0, 0.8);"
    "   color = clamp(dot_nl * color + specular, 0.0, 1.0);"
    //"   color = clamp(specular, 0.0, 1.0);"
    "   fragment_color = color;"
    //"   fragment_color = vec4(float_vr, 0.0, 0.0, 1.0);"
    //"   fragment_color = viewdir;"
    "}";

const char* OpenGlErrorToString(GLenum error) {
  switch (error) {
    case GL_NO_ERROR:
      return "GL_NO_ERROR";
      break;
    case GL_INVALID_ENUM:
      return "GL_INVALID_ENUM";
      break;
    case GL_INVALID_VALUE:
      return "GL_INVALID_VALUE";
      break;
    case GL_INVALID_OPERATION:
      return "GL_INVALID_OPERATION";
      break;
    case GL_OUT_OF_MEMORY:
      return "GL_OUT_OF_MEMORY";
      break;
    default:
      return "Unknown Error";
      break;
  }
  return "Unicorns Exist";
}

#define CHECK_SUCCESS(x) \
  if (!(x)) {            \
    glfwTerminate();     \
    exit(EXIT_FAILURE);  \
  }

#define CHECK_GL_SHADER_ERROR(id)                                           \
  {                                                                         \
    GLint status = 0;                                                       \
    GLint length = 0;                                                       \
    glGetShaderiv(id, GL_COMPILE_STATUS, &status);                          \
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);                         \
    if (!status) {                                                          \
      std::string log(length, 0);                                           \
      glGetShaderInfoLog(id, length, nullptr, &log[0]);                     \
      std::cerr << "Line :" << __LINE__ << " OpenGL Shader Error: Log = \n" \
                << &log[0];                                                 \
      glfwTerminate();                                                      \
      exit(EXIT_FAILURE);                                                   \
    }                                                                      \
  }

#define CHECK_GL_PROGRAM_ERROR(id)                                           \
  {                                                                          \
    GLint status = 0;                                                        \
    GLint length = 0;                                                        \
    glGetProgramiv(id, GL_LINK_STATUS, &status);                             \
    glGetProgramiv(id, GL_INFO_LOG_LENGTH, &length);                         \
    if (!status) {                                                           \
      std::string log(length, 0);                                            \
      glGetProgramInfoLog(id, length, nullptr, &log[0]);                     \
      std::cerr << "Line :" << __LINE__ << " OpenGL Program Error: Log = \n" \
                << &log[0];                                                  \
      glfwTerminate();                                                       \
      exit(EXIT_FAILURE);                                                    \
    }                                                                        \
  }

#define CHECK_GL_ERROR(statement)                                             \
  {                                                                           \
    { statement; }                                                            \
    GLenum error = GL_NO_ERROR;                                               \
    if ((error = glGetError()) != GL_NO_ERROR) {                              \
      std::cerr << "Line :" << __LINE__ << " OpenGL Error: code  = " << error \
                << " description =  " << OpenGlErrorToString(error);          \
      glfwTerminate();                                                        \
      exit(EXIT_FAILURE);                                                     \
    }                                                                         \
  }

class GLShader {
    public:
        enum shader_type {
            VERTEX,
            GEOMETRY,
            FRAGMENT
        };
    private:
        GLuint shader_id;
        shader_type type;
        const char* shader_source;
    public:
        GLShader(const char* source, shader_type s_t) :
                shader_source(source), type(s_t){
            if (s_t == GLShader::VERTEX) {
                CHECK_GL_ERROR(shader_id = glCreateShader(GL_VERTEX_SHADER));
            }
            if (s_t == GLShader::GEOMETRY) {
                CHECK_GL_ERROR(shader_id = glCreateShader(GL_GEOMETRY_SHADER));
            }
            if (s_t == GLShader::FRAGMENT) {
                CHECK_GL_ERROR(shader_id = glCreateShader(GL_FRAGMENT_SHADER));
            }
            CHECK_GL_ERROR(glShaderSource(shader_id, 1, &source, nullptr));
            glCompileShader(shader_id);
            CHECK_GL_SHADER_ERROR(shader_id);
        }
        GLuint ID(){ return shader_id; }
        shader_type Type(){ return type; }
};

class GLProgram {
    private:
        bool v_shader_ready = false;
        bool g_shader_ready = false;
        bool f_shader_ready = false;
        GLuint program_id;
        GLuint v_shader_loc;
        GLuint g_shader_loc;
        GLuint f_shader_loc;
        std::map<std::string, GLuint> uniforms;

        int vert_attribs;
        std::vector<size_t> vert_attrib_sizes;
        int frag_attribs;
        std::vector<size_t> frag_attrib_sizes;

        bool HasUniform(std::string uniform_name){
            return uniforms.count(uniform_name) > 0;
        }
    public:
        GLProgram(GLShader vertex, GLShader geometry, GLShader fragment) :
                v_shader_loc(vertex.ID()),
                g_shader_loc(geometry.ID()),
                f_shader_loc(fragment.ID()){
            // program_id = 0;
            CHECK_GL_ERROR(program_id = glCreateProgram());
            if (vertex.Type() == GLShader::VERTEX) {
                CHECK_GL_ERROR(glAttachShader(program_id, vertex.ID()));
                v_shader_ready = true;
            }
            if (geometry.Type() == GLShader::GEOMETRY) {
                CHECK_GL_ERROR(glAttachShader(program_id, geometry.ID()));
                g_shader_ready = true;
            }
            if (fragment.Type() == GLShader::FRAGMENT) {
                CHECK_GL_ERROR(glAttachShader(program_id, fragment.ID()));
                f_shader_ready = true;
            }

            // Bind attributes.
            vert_attribs = 1;
            frag_attribs = 1;

            CHECK_GL_ERROR(glBindAttribLocation(program_id, 0, "vertex_position"));
            CHECK_GL_ERROR(glBindFragDataLocation(program_id, 0, "fragment_color"));
            glLinkProgram(program_id);
            CHECK_GL_PROGRAM_ERROR(program_id);
        }

        GLuint GetId(){ return program_id; }

        bool ReadyProgram(){
            // There are some conditions to check here before you just ready
            // the program. Such as whether attributes are set up properly?
            // Something like that...
            if (!v_shader_ready){
                std::cout << "Vertex shader invalid\n";
                return false;
            }
            if (!g_shader_ready){
                std::cout << "Geometryshader invalid\n";
                return false;
            }
            if (!f_shader_ready){
                std::cout << "Fragment shader invalid\n";
                return false;
            }
            CHECK_GL_ERROR(glUseProgram(program_id));
            return true;
        }

        // Maybe make these actually do something eventually.
        void AddVertAttrib(const std::string& attrib_name){
            CHECK_GL_ERROR(glBindAttribLocation(program_id, vert_attribs, attrib_name.c_str()));
            vert_attribs++;
        }

        void AddFragAttrib(const std::string& attrib_name){
            CHECK_GL_ERROR(glBindFragDataLocation(program_id, frag_attribs, attrib_name.c_str()));
            frag_attribs++;
        }

        GLuint AddUniform(const std::string uniform_name){
            // Get the uniform locations.
            GLint uniform_loc = 0;
            CHECK_GL_ERROR(uniform_loc = glGetUniformLocation(program_id, uniform_name.c_str()));
            uniforms[uniform_name] = uniform_loc;
            return uniform_loc;
        }

        bool SetUniform(const std::string uniform_name, int i){
            if (!HasUniform(uniform_name)){
                return false;
            }
            CHECK_GL_ERROR(glUniform1i(uniforms[uniform_name], i));
        }

        bool SetUniform(const std::string uniform_name, float f){
            if (!HasUniform(uniform_name)){
                return false;
            }
            CHECK_GL_ERROR(glUniform1f(uniforms[uniform_name], f));
        }

        bool SetUniform(const std::string uniform_name, glm::vec3& vec){
            if (!HasUniform(uniform_name)){
                return false;
            }
            CHECK_GL_ERROR(glUniform3fv(uniforms[uniform_name], 1, &vec[0]));
        }

        bool SetUniform(const std::string uniform_name, glm::vec4& vec){
            if (!HasUniform(uniform_name)){
                return false;
            }
            CHECK_GL_ERROR(glUniform4fv(uniforms[uniform_name], 1, &vec[0]));
        }

        bool SetUniform(const std::string uniform_name, glm::mat4& mat){
            if (!HasUniform(uniform_name)){
                return false;
            }
            CHECK_GL_ERROR(
                glUniformMatrix4fv(uniforms[uniform_name], 1, GL_FALSE, &mat[0][0]));
        }
};

void ErrorCallback(int error, const char* description) {
  std::cerr << "GLFW Error: " << description << "\n";
}

void KeyCallback(GLFWwindow* window, int key, int scancode, int action,
                 int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GL_TRUE);
  else if (key == GLFW_KEY_W && action != GLFW_RELEASE) {
      if (fps_mode){
          eye = eye + zoom_speed * look;
      } else {
          glm::vec3 center = eye + camera_distance * look;
          camera_distance -= zoom_speed;
          eye = center - camera_distance * look;
      }
  } else if (key == GLFW_KEY_S && action != GLFW_RELEASE) {
    // Fix me.
      if (fps_mode){
          eye = eye - zoom_speed * look;
      } else {
          glm::vec3 center = eye + camera_distance * look;
          camera_distance += zoom_speed;
          eye = center - camera_distance * look;
      }
  } else if (key == GLFW_KEY_A && action != GLFW_RELEASE) {
    // Fix me.
    glm::vec3 tan = glm::cross(up, look);
    eye = eye + tan * pan_speed;
  } else if (key == GLFW_KEY_D && action != GLFW_RELEASE) {
    // Fix me.
    glm::vec3 tan = glm::cross(up, look);
    eye = eye - tan * pan_speed;
  } else if (key == GLFW_KEY_LEFT && action != GLFW_RELEASE) {
      up = glm::rotate(up, -roll_speed, look);
    // Fix me.
  } else if (key == GLFW_KEY_RIGHT && action != GLFW_RELEASE) {
      up = glm::rotate(up, roll_speed, look);
    // Fix me.
  } else if (key == GLFW_KEY_DOWN && action != GLFW_RELEASE) {
    // Fix me.
    //eye = eye - glm::vec3(0, 1, 0) * pan_speed;
    eye = eye - up * pan_speed;
  } else if (key == GLFW_KEY_UP && action != GLFW_RELEASE) {
    //eye = eye + glm::vec3(0, 1, 0) * pan_speed;
    eye = eye + up * pan_speed;
        //glm::vec4 neweye = gLm::
    // Fix me.
  } else if (key == GLFW_KEY_C && action != GLFW_RELEASE)
    fps_mode = !fps_mode;
  else if (key == GLFW_KEY_0 && action != GLFW_RELEASE) {
  } else if (key == GLFW_KEY_1 && action != GLFW_RELEASE) {
    // Fix me.
  } else if (key == GLFW_KEY_2 && action != GLFW_RELEASE) {
    // Fix me.
  } else if (key == GLFW_KEY_3 && action != GLFW_RELEASE) {
    // Fix me.
  } else if (key == GLFW_KEY_4 && action != GLFW_RELEASE) {
    // Fix me.
  }
}

void MousePosCallback(GLFWwindow* window, double mouse_x, double mouse_y) {
  last_x = current_x;
  last_y = current_y;
  current_x = mouse_x;
  current_y = mouse_y;
  float delta_x = current_x - last_x;
  float delta_y = current_y - last_y;
    if (sqrt(delta_x * delta_x + delta_y * delta_y) < 1e-15) return;
    if (drag_state && current_button == GLFW_MOUSE_BUTTON_LEFT) {
        glm::vec3 at = eye + look * camera_distance;
        glm::mat4 view_matrix = glm::lookAt(eye, eye + camera_distance * look, up);
        glm::vec3 mouse_direction =
            glm::normalize(glm::vec3(delta_x, -delta_y, 0.0f));
        //glm::vec3 mouse_dir_world = glm::vec3(
        //    glm::inverse(view_matrix) *
        //    (glm::inverse(projection_matrix) * glm::vec4(mouse_direction, 0.0f)));

        glm::vec3 eye_axis = glm::normalize(glm::cross(mouse_direction, glm::vec3(0,0,1)));
        glm::vec4 world_axis = glm::inverse(view_matrix) * glm::vec4(eye_axis, 0.0f);
        look = glm::normalize(glm::rotate(look, rotation_speed, glm::vec3(world_axis)));
        if(!fps_mode) {
            eye = at - look*camera_distance;
        }
        
        up =glm::rotate(up, rotation_speed, glm::vec3(world_axis));
        


        //glm::vec3 rotate_axis = glm::cross(look, mouse_dir_world);
        //look = glm::rotate(look, rotation_speed, rotate_axis);
        //up = glm::rotate(up, rotation_speed, rotate_axis);
        //eye = at - look * camera_distance;
        //eye = glm::rotate(eye, rotation_speed, rotate_axis);
        //look = (at - eye) / camera_distance;
        //glm::vec3 tan = glm::normalize(glm::cross(look, up));
        //up = glm::normalize(glm::cross(tan, look));
        // Fix me.
  } else if (drag_state && current_button == GLFW_MOUSE_BUTTON_RIGHT) {
      glm::vec3 center = eye + camera_distance * look;
      if (delta_y > 0){
          camera_distance -= zoom_speed;
      } else if (delta_y < 0){
          camera_distance += zoom_speed;
      }
      eye = center - camera_distance * look;
  }
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  drag_state = (action == GLFW_PRESS);
  current_button = button;
}

int main(int argc, char* argv[]) {
    if (!glfwInit()) exit(EXIT_FAILURE);

    if (argc != 3){
        std::cout<<"Invalid args.\n";
        std::cout<<"Usage: ./runit.sh dimension maxraindrops"<<std::endl;
        exit(EXIT_SUCCESS);
    }
  
    glfwSetErrorCallback(ErrorCallback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(window_width, window_height,
                                          &window_title[0], nullptr, nullptr);
    CHECK_SUCCESS(window != nullptr);
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    CHECK_SUCCESS(glewInit() == GLEW_OK);
    glGetError();  // clear GLEW's error for it
  
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MousePosCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSwapInterval(1);
    const GLubyte* renderer = glGetString(GL_RENDERER);  // get renderer string
    const GLubyte* version = glGetString(GL_VERSION);    // version as a string
    std::cout << "Renderer: " << renderer << "\n";
    std::cout << "OpenGL version supported:" << version << "\n";
    // Construction of box
    const float box_vertices [96] = {
                                // exterior vertices
                                -2.0f, -2.0f, -2.0f, 1.0f,
                                -2.0f, -2.0f, 2.0f, 1.0f,
                                -2.0f, 0.0f, -2.0f, 1.0f,
                                -2.0f, 0.0f, 2.0f, 1.0f,
                                2.0f, -2.0f, -2.0f, 1.0f,
                                2.0f, -2.0f, 2.0f, 1.0f,
                                2.0f, 0.0f, -2.0f, 1.0f,
                                2.0f, 0.0f, 2.0f, 1.0f,
                                // interior vertices
                                -1.8f, -2.0f, -1.8f, 1.0f,
                                -1.8f, -2.0f, 1.8f, 1.0f,
                                -1.8f, 0.0f, -1.8f, 1.0f,
                                -1.8f, 0.0f, 1.8f, 1.0f,
                                1.8f, -2.0f, -1.8f, 1.0f,
                                1.8f, -2.0f, 1.8f, 1.0f,
                                1.8f, 0.0f, -1.8f, 1.0f,
                                1.8f, 0.0f, 1.8f, 1.0f,
                                // top vertices
                                -1.8, 0.0f, -2.0f, 1.0f,
                                -1.8, 0.0f, 2.0f, 1.0f,
                                1.8, 0.0f, -2.0f, 1.0f,
                                1.8, 0.0f, 2.0f, 1.0f,
                                // bottom vertices
                                -1.8, -1.90f, -1.8f, 1.0f,
                                -1.8, -1.90f, 1.8f, 1.0f,
                                1.8, -1.90f, -1.8f, 1.0f,
                                1.8, -1.90f, 1.8f, 1.0f};
    int box_vert_len = 8;
    const int box_faces [78] = {
                                // exterior vertices
                                5, 7, 1,
                                3, 1, 7,
                                4, 6, 5,
                                7, 5, 6,
                                1, 3, 0,
                                2, 0, 3,
                                4, 0, 6,
                                2, 6, 0,
                                // interior vertices
                                box_vert_len + 1, box_vert_len + 7, box_vert_len + 5,
                                box_vert_len + 7, box_vert_len + 1, box_vert_len + 3,
                                box_vert_len + 5, box_vert_len + 6, box_vert_len + 4,
                                box_vert_len + 6, box_vert_len + 5, box_vert_len + 7,
                                box_vert_len + 0, box_vert_len + 3, box_vert_len + 1,
                                box_vert_len + 3, box_vert_len + 0, box_vert_len + 2,
                                box_vert_len + 6, box_vert_len + 0, box_vert_len + 4,
                                box_vert_len + 0, box_vert_len + 6, box_vert_len + 2,
                                // box top faces
                                3, 16, 2,
                                16, 3, 17,
                                15, 11, 17,
                                17, 19, 15,
                                19, 6, 18,
                                6, 19, 7,
                                18, 16, 10,
                                10, 14, 18,
                                // box bottom faces
                                22, 20, 21,
                                21, 23, 22};
    // Water construction
    // TODO: HEAP ALLOCATE STUFF
    printf("sizeof(float) == %lu\nsizeof(uint32_t) == %lu\n", sizeof(float), sizeof(uint32_t));
    float water_height = -0.3f;
    int dimension = atoi(argv[1]);
    int dimension_plus = dimension + 1;
    int dimension_2 = dimension * dimension;
    int dimension_plus_2 = dimension_plus * dimension_plus;
    uint32_t * water_faces    = new uint32_t [dimension_2 * 8];
    float * float_arr         = new float    [dimension_plus_2 * 11];
    float * water_vertices    = float_arr;                          // size: dimension_plus_2 * 4
    float * water_height_curr = float_arr + dimension_plus_2 * 4;   // size: dimension_plus_2
    float * water_height_prev = float_arr + dimension_plus_2 * 5;   // size: dimension_plus_2
    float * water_forces      = float_arr + dimension_plus_2 * 6;   // size: dimension_plus_2
    float * water_vel_prev    = float_arr + dimension_plus_2 * 7;   // size: dimension_plus_2 * 2
    float * water_vel_curr    = float_arr + dimension_plus_2 * 9;   // size: dimension_plus_2 * 2
    float water_corner = -1.8f, water_len = 3.6f, dwater = water_len / dimension;
    int grid_i = 0, index_i = 0, vert_i = 0, vel_i = 0;
    for (int j = 0; j < dimension_plus; j++) {
        for (int i = 0; i < dimension_plus; i++) {
            // water vertices
            water_vertices[vert_i]     = water_corner + dwater * i;
            water_vertices[vert_i + 1] = water_height;
            water_vertices[vert_i + 2] = water_corner + dwater * j;
            water_vertices[vert_i + 3] = 1.0f;

            water_forces[grid_i] = 0.0f;

            water_vel_curr[vel_i] = 0.f;
            water_vel_prev[vel_i] = 0.f;
            water_vel_curr[vel_i + 1] = 0.f;
            water_vel_prev[vel_i + 1] = 0.f;

            water_height_curr[grid_i] = 0.0f;
            water_height_prev[grid_i] = 0.0f;

            vert_i += 4;
            vel_i += 2;
            grid_i++;
            // water faces
            if (i < dimension && j < dimension) {
                water_faces[index_i]     = i * dimension_plus + j + 1;
                water_faces[index_i + 1] = i * dimension_plus + j;
                water_faces[index_i + 2] = (i + 1) * dimension_plus + j;
                water_faces[index_i + 3] = (i + 1) * dimension_plus + j;
                water_faces[index_i + 4] = (i + 1) * dimension_plus + j + 1;
                water_faces[index_i + 5] = i * dimension_plus + j + 1;
                index_i += 6;
            }
        }
    }
    // Rain construction
    std::vector<glm::vec4> rain_drops;
    std::vector<float> rain_speeds;
    std::vector<int> rain_indices;
    float range = 3.0f;
    float precision = 1000.0f;
    int xpos = rand() % 1000;
    int zpos = rand() % 1000;
    int maxdrops = atoi(argv[2]);
    for (int i=0; i < maxdrops; i++){
        rain_indices.push_back(i);
    }
    rain_speeds.push_back(0.0f);
    // Plane construction
    const float plane_vertices [20] = {0.f, -2.f, 0.f, 1.f,
                                       99999.f, 0.f, 0.f, 0.f,
                                       0.f, 0.f, 99999.f, 0.f,
                                       -99999.f, 0.f, 0.f, 0.f,
                                       0.f, 0.f, -99999.f, 0.f};
    const int plane_faces [12] = {0, 2, 1,
                                  0, 3, 2,
                                  0, 4, 3,
                                  0, 1, 4};
    // Setup our VAOs.
    CHECK_GL_ERROR(glGenVertexArrays(kNumVaos, array_objects));
    // Setup the box array object.
    CHECK_GL_ERROR(glBindVertexArray(array_objects[kBoxVao]));
    // Generate buffer objects
    CHECK_GL_ERROR(glGenBuffers(kNumVbos, &buffer_objects[kBoxVao][0]));
    // Setup vertex data in a VBO.
    CHECK_GL_ERROR(
        glBindBuffer(GL_ARRAY_BUFFER, buffer_objects[kBoxVao][kVertexBuffer]));
    CHECK_GL_ERROR(glBufferData(GL_ARRAY_BUFFER,
                                sizeof(float) * 96,
                                box_vertices, GL_STATIC_DRAW));
    CHECK_GL_ERROR(glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0));
    CHECK_GL_ERROR(glEnableVertexAttribArray(0));
    // Setup element array buffer.
    CHECK_GL_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                buffer_objects[kBoxVao][kIndexBuffer]));
    CHECK_GL_ERROR(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                sizeof(uint32_t) * 78,
                                box_faces, GL_STATIC_DRAW));
    // Setup the water array object.
    CHECK_GL_ERROR(glBindVertexArray(array_objects[kWaterVao]));
    // Generate buffer objects
    CHECK_GL_ERROR(glGenBuffers(kNumVbos, &buffer_objects[kWaterVao][0]));
    // Setup vertex data in a VBO.
    CHECK_GL_ERROR(
        glBindBuffer(GL_ARRAY_BUFFER, buffer_objects[kWaterVao][kVertexBuffer]));
    CHECK_GL_ERROR(glBufferData(GL_ARRAY_BUFFER,
                                sizeof(float) * dimension_plus_2 * 4,
                                water_vertices, GL_STATIC_DRAW));
    CHECK_GL_ERROR(glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0));
    CHECK_GL_ERROR(glEnableVertexAttribArray(0));
    // Setup vertex data in a VBO.
    CHECK_GL_ERROR(
        glBindBuffer(GL_ARRAY_BUFFER, buffer_objects[kWaterVao][kVertAttr1]));
    CHECK_GL_ERROR(glBufferData(GL_ARRAY_BUFFER,
                                sizeof(float) * dimension_plus_2,
                                water_height_curr, GL_STATIC_DRAW));
    CHECK_GL_ERROR(glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, 0));
    CHECK_GL_ERROR(glEnableVertexAttribArray(1));
    // Setup element array buffer.
    CHECK_GL_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                buffer_objects[kWaterVao][kIndexBuffer]));
    CHECK_GL_ERROR(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                sizeof(uint32_t) * dimension_2 * 6,
                                water_faces, GL_STATIC_DRAW));
    //setup the rain
    CHECK_GL_ERROR(glBindVertexArray(array_objects[kRainVao]));
    // Generate buffer objects
    CHECK_GL_ERROR(glGenBuffers(kNumVbos, &buffer_objects[kRainVao][0]));
    // Setup vertex data in a VBO.
    CHECK_GL_ERROR(
        glBindBuffer(GL_ARRAY_BUFFER, buffer_objects[kRainVao][kVertexBuffer]));
    CHECK_GL_ERROR(glBufferData(GL_ARRAY_BUFFER,
                                sizeof(float) * rain_drops.size() * 4,
                                &rain_drops[0], GL_STATIC_DRAW));
    CHECK_GL_ERROR(glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0));
    CHECK_GL_ERROR(glEnableVertexAttribArray(0));
    // Setup element array buffer.
    CHECK_GL_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                buffer_objects[kRainVao][kIndexBuffer]));
    CHECK_GL_ERROR(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                sizeof(uint32_t) * rain_indices.size(),
                                &rain_indices[0], GL_STATIC_DRAW));
    //setup the plane project
    CHECK_GL_ERROR(glBindVertexArray(array_objects[kPlaneVao]));
    // Generate buffer objects
    CHECK_GL_ERROR(glGenBuffers(kNumVbos, &buffer_objects[kPlaneVao][0]));
    // Setup vertex data in a VBO.
    CHECK_GL_ERROR(
        glBindBuffer(GL_ARRAY_BUFFER, buffer_objects[kPlaneVao][kVertexBuffer]));
    CHECK_GL_ERROR(glBufferData(GL_ARRAY_BUFFER,
                                sizeof(float) * 20,
                                plane_vertices, GL_STATIC_DRAW));
    CHECK_GL_ERROR(glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0));
    CHECK_GL_ERROR(glEnableVertexAttribArray(0));
    // Setup element array buffer.
    CHECK_GL_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                buffer_objects[kPlaneVao][kIndexBuffer]));
    CHECK_GL_ERROR(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                sizeof(uint32_t) * 12,
                                plane_faces, GL_STATIC_DRAW));

    GLShader vert = GLShader(vertex_shader, GLShader::VERTEX);
    GLShader point_vert = GLShader(point_vertex_shader,  GLShader::VERTEX);
    GLShader water_vert = GLShader(water_vertex_shader, GLShader::VERTEX);
    //GLShader texture_v_shader = GLShader(texture_vertex_shader, GLShader::VERTEX);

    GLShader geom = GLShader(geometry_shader, GLShader::GEOMETRY);
    GLShader point_geom = GLShader(point_geometry_shader,  GLShader::GEOMETRY);
    GLShader water_geom = GLShader(water_geometry_shader,  GLShader::GEOMETRY);

    GLShader frag = GLShader(fragment_shader,  GLShader::FRAGMENT);
    GLShader point_frag = GLShader(point_fragment_shader,  GLShader::FRAGMENT);
    GLShader water_frag = GLShader(water_fragment_shader, GLShader::FRAGMENT);
    //GLShader phong_f_shader = GLShader(phong_fragment_shader, GLShader::FRAGMENT);
    //GLShader texture_f_shader = GLShader(texture_fragment_shader, GLShader::FRAGMENT);


    // basic program
    GLProgram basic_program(vert, geom, frag);
    basic_program.AddUniform("projection");
    basic_program.AddUniform("view");
    basic_program.AddUniform("light_position");
    basic_program.AddUniform("diffuse_color");
    basic_program.AddUniform("alpha");
    // water program
    GLProgram water_program(water_vert, water_geom, water_frag);
    water_program.AddVertAttrib("height");
    water_program.AddUniform("projection");
    water_program.AddUniform("view");
    water_program.AddUniform("light_position");
    // water_program.AddUniform("diffuse_color");
    water_program.AddUniform("look");
    // water_program.AddUniform("alpha");
    // water_program.AddUniform("water_len");
    // water_program.AddUniform("water_corner");
    // water_program.AddUniform("dimension");
    // water_program.AddUniform("t");
    // rain program
    GLProgram rain_program(point_vert, point_geom, point_frag);
    rain_program.AddUniform("projection");
    rain_program.AddUniform("view");
    // lol idk
    glfwSwapInterval(1);
    // Important variables
    float aspect = 0.0f;
    glm::vec4 light_position = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec3 plane_color = glm::vec3(0.0f, 0.6f, 0.0f);
    glm::vec3 box_color = glm::vec3(0.4f, 0.2f, 0.0f);
    glm::vec3 water_color = glm::vec3(0.0f, 0.4f, 1.0f);
    // rain variables
    clock_t prev = clock();
    float gravity = 10.0f; //lol
    float forceconst = 2.0f;
    float H = 1.7f;
    float time = 0;
    float delta = 0.3f;
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Setup some basic window stuff.
        glfwGetFramebufferSize(window, &window_width, &window_height);
        glViewport(0, 0, window_width, window_height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_MULTISAMPLE);
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_LESS);

        aspect = static_cast<float>(window_width) / window_height;
        glm::mat4 projection_matrix = glm::perspective(45.0f, aspect, 0.0001f, 1000.0f);
        glm::mat4 view_matrix = glm::lookAt(eye, eye + camera_distance * look, up);

        clock_t curr = clock();
        //float diff = ((float)(curr - prev)/CLOCKS_PER_SEC);
        float diff = 0.003333f / 2.0f;
        time += diff;
        prev = curr;
        //if (rain_drops.size() > 0){
        //    int i = 0;
        //    rain_speeds[i] += gravity * diff;
        //    rain_drops[i][1] -= rain_speeds[i] * diff;
        //    std::cout << i << ' ' << rain_speeds[i] << '\n';
        //}
        int k = 0;
        while (k < rain_drops.size()){
            if (rain_drops[k][1] < water_height){
                int i = (int)((rain_drops[k][0] - water_corner) / dwater);
                int j = (int)((rain_drops[k][2] - water_corner) / dwater);
                int index = j * (dimension + 1) + i;
                water_forces[index] = forceconst;
                rain_drops.erase(rain_drops.begin() + k);
                rain_speeds.erase(rain_speeds.begin() + k);
            } else {
                rain_speeds[k] += gravity * diff;
                rain_drops[k][1] -= rain_speeds[k] * diff;
                k++;
            }
        }
        int rain_check = rand() % 1000;
        if (rain_drops.size() < maxdrops && rain_check > 950){
            float range = 3.0f;
            float precision = 1000.0f;
            int xpos = rand() % 1000;
            int zpos = rand() % 1000;
            glm::vec4 newpos
                (xpos / precision * range - 1.5f, 5.0f,
                 zpos / precision * range - 1.5f, 1.0f);
            rain_drops.push_back(newpos);
            rain_speeds.push_back(2.0f);
        }
        vel_i = 0;
        grid_i = 0;
        for (int j = 0; j < dimension_plus; j++) {
            for (int i = 0; i < dimension_plus; i++) {
                water_vel_prev[vel_i] = water_vel_curr[vel_i];
                water_vel_prev[vel_i + 1] = water_vel_curr[vel_i];
                vel_i += 2;

                water_height_prev[grid_i] = water_height_curr[grid_i];
                grid_i++;
            }
        }
        float double_dwater = 2 * dwater;
        vel_i = 0, grid_i = 0;
        for (int j = 0; j < dimension_plus; j++){
            for (int i = 0; i < dimension_plus; i++){
                float height_grad_i = 0;
                float height_grad_j = 0;
                float force_grad_i = 0;
                float force_grad_j = 0;
                glm::vec2 dudx;
                glm::vec2 dvdx;
                glm::vec2 prev_vel       = glm::vec2(water_vel_prev[vel_i], water_vel_prev[vel_i + 1]),
                          prev_vel_left  = glm::vec2(water_vel_prev[vel_i - 2], water_vel_prev[vel_i - 1]),
                          prev_vel_right = glm::vec2(water_vel_prev[vel_i + 2], water_vel_prev[vel_i + 3]),
                          prev_vel_down  = glm::vec2(water_vel_prev[vel_i + dimension_plus * 2],
                                                     water_vel_prev[vel_i + dimension_plus * 2 + 1]),
                          prev_vel_up    = glm::vec2(water_vel_prev[vel_i - dimension_plus * 2],
                                                      water_vel_prev[vel_i - dimension_plus * 2 + 1]);

                float height       = water_height_prev[grid_i];
                float height_left  = water_height_prev[grid_i - 1];
                float height_right = water_height_prev[grid_i + 1];
                float height_down  = water_height_prev[grid_i + dimension_plus];
                float height_up    = water_height_prev[grid_i - dimension_plus];

                float force       = water_forces[grid_i];
                float force_left  = water_forces[grid_i - 1];
                float force_right = water_forces[grid_i + 1];
                float force_down  = water_forces[grid_i + dimension_plus];
                float force_up    = water_forces[grid_i - dimension_plus];
                if (i == 0){
                    height_grad_i = (height_right - height) / (double_dwater);
                    force_grad_i  = (force_right - force) / (double_dwater);
                    dudx          = (prev_vel_right - prev_vel) / (double_dwater);
                } else if (i == dimension){
                    height_grad_i = (height - height_left) / (double_dwater);
                    force_grad_i  = (force - force_left) / (double_dwater);
                    dudx = (prev_vel - prev_vel_left) / (double_dwater);
                } else {
                    height_grad_i = (height_right - height_left) / (double_dwater);
                    force_grad_i  = (force_right - force_left) / (double_dwater);
                    dudx          = (prev_vel_right - prev_vel_left) / (double_dwater);
                }
                if (j == 0){
                    height_grad_j = (height_down - height) / (double_dwater);
                    force_grad_j  = (force_down - force) / (double_dwater);
                    dvdx = (prev_vel_down - prev_vel) / (double_dwater);
                } else if (j == dimension){
                    height_grad_j = (height - height_up) / (double_dwater);
                    force_grad_j  = (force - force_up) / (double_dwater);
                    dvdx = (prev_vel - prev_vel_up) / (double_dwater);
                } else {
                    height_grad_j = (height_down - height_up) / (double_dwater);
                    force_grad_j  = (force_down - force_up) / (double_dwater);
                    dvdx = (prev_vel_down - prev_vel_up) / (double_dwater);
                }
                glm::vec2 height_gradient (height_grad_i, height_grad_j);
                glm::vec2 force_gradient (force_grad_i, force_grad_j);
                glm::vec2 u_dudx = water_vel_prev[vel_i] * dudx;
                glm::vec2 v_dvdx = water_vel_prev[vel_i + 1] * dvdx;
                glm::vec2 curr_vel = (-(gravity + force) * height_gradient
                                      - 1.7f * force_gradient - u_dudx - v_dvdx) * diff
                                     + prev_vel;
                water_vel_curr[vel_i] = curr_vel[0];
                water_vel_curr[vel_i + 1] = curr_vel[1];
                grid_i++;
                vel_i += 2;
            }
        }
        vel_i = 0;
        grid_i = 0;
        for (int j = 1; j < dimension; j++){
            for (int i = 1; i < dimension; i++){
                glm::vec2 dudx;
                glm::vec2 dvdx;
                glm::vec2 vel       = glm::vec2(water_vel_curr[vel_i], water_vel_curr[vel_i + 1]),
                          vel_left  = glm::vec2(water_vel_curr[vel_i - 2], water_vel_curr[vel_i - 1]),
                          vel_right = glm::vec2(water_vel_curr[vel_i + 2], water_vel_curr[vel_i + 3]),
                          vel_down  = glm::vec2(water_vel_curr[vel_i + dimension_plus * 2],
                                                water_vel_curr[vel_i + dimension_plus * 2 + 1]),
                          vel_up    = glm::vec2(water_vel_curr[vel_i - dimension_plus * 2],
                                                      water_vel_curr[vel_i - dimension_plus * 2 + 1]);

                float height       = water_height_prev[grid_i];
                float height_left  = water_height_prev[grid_i - 1];
                float height_right = water_height_prev[grid_i + 1];
                float height_down  = water_height_prev[grid_i + dimension_plus];
                float height_up    = water_height_prev[grid_i - dimension_plus];
                // TODO: water pressure or some shit 
                // water_forces[grid_i] = 0;
                float vel_grad_x = (vel_right[0] - vel_left[0]) / (double_dwater);
                float u_dhdx = vel[0]  * (height_right - height_left) / (double_dwater);
                float vel_grad_y =
                    (vel_down[1] - vel_up[1]) / (double_dwater);
                float v_dhdy = vel[1] * (height_up - height_down) / (double_dwater);
                water_height_curr[grid_i] =
                    (-(water_height_prev[grid_i] + H) * (vel_grad_x + vel_grad_y)
                        - u_dhdx - v_dhdy) * diff
                    + water_height_prev[grid_i];
                grid_i++;
                vel_i += 2;
            }
        }
        if (basic_program.ReadyProgram()){
            // Set uniforms
            basic_program.SetUniform("projection", projection_matrix);
            basic_program.SetUniform("view", view_matrix);
            basic_program.SetUniform("light_position", light_position);
            // Draw box.
            basic_program.SetUniform("diffuse_color", box_color);
            CHECK_GL_ERROR(glBindVertexArray(array_objects[kBoxVao]));
            CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES, 78,
                                          GL_UNSIGNED_INT, 0));
            // Draw basic.
            basic_program.SetUniform("diffuse_color", plane_color);
            CHECK_GL_ERROR(glBindVertexArray(array_objects[kPlaneVao]));
            CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES, 12,
                                          GL_UNSIGNED_INT, 0));
        } else {
            std::cout << "Shit fucked up\n";
        }
        if (water_program.ReadyProgram()){
            // Set uniforms
            water_program.SetUniform("projection", projection_matrix);
            water_program.SetUniform("view", view_matrix);
            water_program.SetUniform("light_position", light_position);
            water_program.SetUniform("look", look);
            // Draw water
            water_program.SetUniform("diffuse_color", water_color);
            CHECK_GL_ERROR(glBindVertexArray(array_objects[kWaterVao]));
            CHECK_GL_ERROR(
                glBindBuffer(GL_ARRAY_BUFFER, buffer_objects[kWaterVao][kVertAttr1]));
            // CHECK_GL_ERROR(glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, 0));
            CHECK_GL_ERROR(glBufferData(GL_ARRAY_BUFFER,
                                        sizeof(float) * dimension_plus_2,
                                        water_height_curr, GL_STATIC_DRAW));
            // CHECK_GL_ERROR(glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, 0));
            CHECK_GL_ERROR(glDrawElements(GL_TRIANGLES, dimension_2 * 6,
                                          GL_UNSIGNED_INT, 0));
        } else {
            std::cout << "Shit fucked up\n";
        }

        if (rain_program.ReadyProgram()){
            rain_program.SetUniform("projection", projection_matrix);
            rain_program.SetUniform("view", view_matrix);
            glm::vec4 red = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            //setup the rain
            CHECK_GL_ERROR(glBindVertexArray(array_objects[kRainVao]));
            // Setup vertex data in a VBO.
            CHECK_GL_ERROR(
                glBindBuffer(GL_ARRAY_BUFFER, buffer_objects[kRainVao][kVertexBuffer]));
            CHECK_GL_ERROR(glBufferData(GL_ARRAY_BUFFER,
                                        sizeof(float) * rain_drops.size() * 4,
                                        &rain_drops[0], GL_STATIC_DRAW));
            CHECK_GL_ERROR(glDrawArrays(GL_POINTS, 0, rain_drops.size()));
        } else {
            std::cout << "Shit fucked up during rain program\n";
        }

        // Poll and swap.
        glfwPollEvents();
        glfwSwapBuffers(window);
    }
    delete [] float_arr;
    delete [] water_faces;
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
}

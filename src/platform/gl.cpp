#include "gl.h"
#include "app.h"
#include "util/log.h"
#include "util/load.h"
#include <fstream>


namespace Shaders {

const std::string mesh_v = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 WorldPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;  
    
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
)";

const std::string mesh_f = R"(
#version 330 core
out vec4 FragColor;

in vec3 Normal;  
in vec3 WorldPos;  
  
uniform vec3 lightPos; 
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    FragColor = vec4(Normal, 1.0);
} 
)";

const std::string SkyBox_v = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}  
)";

const std::string SkyBox_f = R"(
#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{    
    FragColor = texture(skybox, TexCoords);
}
)";

}

Shader::Shader() {}

Shader::Shader(std::string vertex_code, std::string fragment_code) { load(vertex_code, fragment_code); }

Shader::Shader(fs::path vertex_path, fs::path fragmentPath)
{
    // 1. retrieve the vertex/fragment source code from filePath
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;
    // ensure ifstream objects can throw exceptions:
    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        // open files
        vShaderFile.open(vertex_path);
        fShaderFile.open(fragmentPath);
        std::stringstream vShaderStream, fShaderStream;
        // read file's buffer contents into streams
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();
        // close file handlers
        vShaderFile.close();
        fShaderFile.close();
        // convert stream into string
        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e)
    {
        fmt::print("ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ\n");
    }
    load(vertexCode, fragmentCode);
}

Shader::Shader(Shader &&src) {
    program = src.program;
    src.program = 0;
    v = src.v;
    src.v = 0;
    f = src.f;
    src.f = 0;
}

void Shader::operator=(Shader &&src) {
    destroy();
    program = src.program;
    src.program = 0;
    v = src.v;
    src.v = 0;
    f = src.f;
    src.f = 0;
}

Shader::~Shader() { destroy(); }

void Shader::bind() const { glUseProgram(program); }

void Shader::destroy() {
    // Hack to let stuff get destroyed for headless mode
    if (!glUseProgram)
        return;

    glUseProgram(0);
    glDeleteShader(v);
    glDeleteShader(f);
    glDeleteProgram(program);
    v = f = program = 0;
}

void Shader::uniform_block(std::string name, GLuint i) const {
    GLuint idx = glGetUniformBlockIndex(program, name.c_str());
    glUniformBlockBinding(program, idx, i);
}

void Shader::uniform(std::string name, int count, const glm::vec2 items[]) const {
    glUniform2fv(loc(name), count, (GLfloat *)items);
}

void Shader::uniform(std::string name, GLfloat fl) const { glUniform1f(loc(name), fl); }

void Shader::uniform(std::string name, const glm::mat4 &mat) const {
    glUniformMatrix4fv(loc(name), 1, GL_FALSE, &mat[0][0]);
}

void Shader::uniform(std::string name, glm::vec3 vec3) const { glUniform3fv(loc(name), 1, &vec3[0]); }

void Shader::uniform(std::string name, glm::vec2 vec2) const { glUniform2fv(loc(name), 1, &vec2[0]); }

void Shader::uniform(std::string name, GLint i) const { glUniform1i(loc(name), i); }

void Shader::uniform(std::string name, GLuint i) const { glUniform1ui(loc(name), i); }

void Shader::uniform(std::string name, bool b) const { glUniform1i(loc(name), b); }

GLuint Shader::loc(std::string name) const { return glGetUniformLocation(program, name.c_str()); }

void Shader::load(std::string vertex_code, std::string fragment_code) {

    v = glCreateShader(GL_VERTEX_SHADER);
    f = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar *vs_c = vertex_code.c_str();
    const GLchar *fs_c = fragment_code.c_str();
    glShaderSource(v, 1, &vs_c, NULL);
    glShaderSource(f, 1, &fs_c, NULL);
    glCompileShader(v);
    glCompileShader(f);

    if (!validate(v)) {
        destroy();
        return;
    }
    if (!validate(f)) {
        destroy();
        return;
    }

    program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glLinkProgram(program);
}

bool Shader::validate(GLuint program) {

    GLint compiled = 0;
    glGetShaderiv(program, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {

        GLint len = 0;
        glGetShaderiv(program, GL_INFO_LOG_LENGTH, &len);

        GLchar *msg = new GLchar[len];
        glGetShaderInfoLog(program, len, &len, msg);

        warn("Shader %d failed to compile: %s", program, msg);
        delete[] msg;

        return false;
    }
    return true;
}




Mesh::Mesh() { create(); }

Mesh::Mesh(std::vector<Vert>&& vertices, std::vector<Index>&& indices) {
    create();
    recreate(std::move(vertices), std::move(indices));
}

Mesh::Mesh(Mesh&& src) {
    vao = src.vao;
    src.vao = 0;
    ebo = src.ebo;
    src.ebo = 0;
    vbo = src.vbo;
    src.vbo = 0;
    dirty = src.dirty;
    src.dirty = true;
    n_elem = src.n_elem;
    src.n_elem = 0;
    _verts = std::move(src._verts);
    _idxs = std::move(src._idxs);
}

void Mesh::operator=(Mesh&& src) {
    destroy();
    vao = src.vao;
    src.vao = 0;
    vbo = src.vbo;
    src.vbo = 0;
    ebo = src.ebo;
    src.ebo = 0;
    dirty = src.dirty;
    src.dirty = true;
    n_elem = src.n_elem;
    src.n_elem = 0;
    _verts = std::move(src._verts);
    _idxs = std::move(src._idxs);
}

Mesh::~Mesh() { destroy(); }

void Mesh::create() {
    // Hack to let stuff get created for headless mode
    if (!glGenVertexArrays)
        return;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (GLvoid*)sizeof(glm::vec3));
    glEnableVertexAttribArray(1);


    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBindVertexArray(0);
}

void Mesh::destroy() {
    // Hack to let stuff get destroyed for headless mode
    if (!glDeleteBuffers)
        return;

    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    ebo = vao = vbo = 0;
}

void Mesh::update() {
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vert) * _verts.size(), _verts.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Index) * _idxs.size(), _idxs.data(),
        GL_DYNAMIC_DRAW);

    glBindVertexArray(0);

    dirty = false;
}

void Mesh::recreate(std::vector<Vert>&& vertices, std::vector<Index>&& indices) {

    dirty = true;
    _verts = std::move(vertices);
    _idxs = std::move(indices);
    n_elem = (GLuint)_idxs.size();
}

GLuint Mesh::tris() const { return n_elem / 3; }

std::vector<Mesh::Vert>& Mesh::edit_verts() {
    dirty = true;
    return _verts;
}

std::vector<Mesh::Index>& Mesh::edit_indices() {
    dirty = true;
    return _idxs;
}

const std::vector<Mesh::Vert>& Mesh::verts() const { return _verts; }

const std::vector<Mesh::Index>& Mesh::indices() const { return _idxs; }

void Mesh::render(Shader& shader) {
    if (dirty)
        update();
    shader.bind();
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, n_elem, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

SkyBox::SkyBox(std::vector<std::string> faces)
{
    cubemapTexture = loadCubemap(faces);
    skyboxShader = Shader{ Shaders::SkyBox_v, Shaders::SkyBox_f };
    create();
}

void SkyBox::create()
{
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

void SkyBox::render()
{
    glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
    skyboxShader.bind();
    auto& app = App::get();
    auto view = glm::mat4(glm::mat3(app.camera.GetViewMatrix())); // remove translation from the view matrix
    skyboxShader.uniform("view", view);
    skyboxShader.uniform("projection", app.M_projection);
    // skybox cube
    glBindVertexArray(skyboxVAO);
    skyboxShader.uniform("skybox", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS); // set depth function back to default
}

SkyBox::~SkyBox()
{
    glDeleteTextures(1, &cubemapTexture);
    glDeleteBuffers(1, &skyboxVBO);
    glDeleteVertexArrays(1, &skyboxVAO);
    skyboxVAO = skyboxVBO = 0;
}

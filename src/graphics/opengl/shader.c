#include "graphics/opengl/opengl.h"
#include "graphics/graphics.h"
#include "resources/shaders.h"
#include "math/math.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static UniformType getUniformType(GLenum type, const char* debug) {
  switch (type) {
    case GL_FLOAT:
    case GL_FLOAT_VEC2:
    case GL_FLOAT_VEC3:
    case GL_FLOAT_VEC4:
      return UNIFORM_FLOAT;

    case GL_INT:
    case GL_INT_VEC2:
    case GL_INT_VEC3:
    case GL_INT_VEC4:
      return UNIFORM_INT;

    case GL_FLOAT_MAT2:
    case GL_FLOAT_MAT3:
    case GL_FLOAT_MAT4:
      return UNIFORM_MATRIX;

    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_2D_ARRAY:
      return UNIFORM_SAMPLER;

    default:
      lovrThrow("Unsupported type for uniform '%s'", debug);
      return UNIFORM_FLOAT;
  }
}

static int getUniformComponents(GLenum type) {
  switch (type) {
    case GL_FLOAT:
    case GL_INT:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_2D_ARRAY:
      return 1;

    case GL_FLOAT_VEC2:
    case GL_INT_VEC2:
    case GL_FLOAT_MAT2:
      return 2;

    case GL_FLOAT_VEC3:
    case GL_INT_VEC3:
    case GL_FLOAT_MAT3:
      return 3;

    case GL_FLOAT_VEC4:
    case GL_INT_VEC4:
    case GL_FLOAT_MAT4:
      return 4;

    default:
      return 1;
  }
}

static GLuint compileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);

  glShaderSource(shader, 1, (const GLchar**) &source, NULL);
  glCompileShader(shader);

  int isShaderCompiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isShaderCompiled);
  if (!isShaderCompiled) {
    int logLength;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    char* log = malloc(logLength);
    glGetShaderInfoLog(shader, logLength, &logLength, log);
    lovrThrow("Could not compile shader %s", log);
  }

  return shader;
}

static GLuint linkShaders(GLuint vertexShader, GLuint fragmentShader) {
  GLuint program = glCreateProgram();

  if (vertexShader) {
    glAttachShader(program, vertexShader);
  }

  if (fragmentShader) {
    glAttachShader(program, fragmentShader);
  }

  glBindAttribLocation(program, LOVR_SHADER_POSITION, "lovrPosition");
  glBindAttribLocation(program, LOVR_SHADER_NORMAL, "lovrNormal");
  glBindAttribLocation(program, LOVR_SHADER_TEX_COORD, "lovrTexCoord");
  glBindAttribLocation(program, LOVR_SHADER_VERTEX_COLOR, "lovrVertexColor");
  glBindAttribLocation(program, LOVR_SHADER_TANGENT, "lovrTangent");
  glBindAttribLocation(program, LOVR_SHADER_BONES, "lovrBones");
  glBindAttribLocation(program, LOVR_SHADER_BONE_WEIGHTS, "lovrBoneWeights");
  glLinkProgram(program);

  int isLinked;
  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
  if (!isLinked) {
    int logLength;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

    char* log = malloc(logLength);
    glGetProgramInfoLog(program, logLength, &logLength, log);
    lovrThrow("Could not link shader %s", log);
  }

  glDetachShader(program, vertexShader);
  glDeleteShader(vertexShader);
  glDetachShader(program, fragmentShader);
  glDeleteShader(fragmentShader);

  return program;
}

Shader* lovrShaderCreate(const char* vertexSource, const char* fragmentSource) {
  Shader* shader = lovrAlloc(sizeof(Shader), lovrShaderDestroy);
  if (!shader) return NULL;

  char source[8192];

  // Vertex
  vertexSource = vertexSource == NULL ? lovrDefaultVertexShader : vertexSource;
  snprintf(source, sizeof(source), "%s%s\n%s", lovrShaderVertexPrefix, vertexSource, lovrShaderVertexSuffix);
  GLuint vertexShader = compileShader(GL_VERTEX_SHADER, source);

  // Fragment
  fragmentSource = fragmentSource == NULL ? lovrDefaultFragmentShader : fragmentSource;
  snprintf(source, sizeof(source), "%s%s\n%s", lovrShaderFragmentPrefix, fragmentSource, lovrShaderFragmentSuffix);
  GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, source);

  // Link
  uint32_t program = linkShaders(vertexShader, fragmentShader);
  shader->program = program;

  gpuUseProgram(program);
  glVertexAttrib4fv(LOVR_SHADER_VERTEX_COLOR, (float[4]) { 1., 1., 1., 1. });
  glVertexAttribI4iv(LOVR_SHADER_BONES, (int[4]) { 0., 0., 0., 0. });
  glVertexAttrib4fv(LOVR_SHADER_BONE_WEIGHTS, (float[4]) { 1., 0., 0., 0. });

  // Uniform introspection
  int32_t uniformCount;
  int textureSlot = 0;
  map_init(&shader->uniforms);
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniformCount);
  for (int i = 0; i < uniformCount; i++) {
    Uniform uniform;
    glGetActiveUniform(program, i, LOVR_MAX_UNIFORM_LENGTH, NULL, &uniform.count, &uniform.glType, uniform.name);

    char* subscript = strchr(uniform.name, '[');
    if (subscript) {
      *subscript = '\0';
    }

    uniform.index = i;
    uniform.location = glGetUniformLocation(program, uniform.name);
    uniform.type = getUniformType(uniform.glType, uniform.name);
    uniform.components = getUniformComponents(uniform.glType);
    uniform.baseTextureSlot = (uniform.type == UNIFORM_SAMPLER) ? textureSlot : -1;

    if (uniform.location == -1) {
      continue;
    }

    switch (uniform.type) {
      case UNIFORM_FLOAT:
        uniform.size = uniform.components * uniform.count * sizeof(float);
        uniform.value.data = calloc(1, uniform.size);
        break;

      case UNIFORM_INT:
        uniform.size = uniform.components * uniform.count * sizeof(int);
        uniform.value.data = calloc(1, uniform.size);
        break;

      case UNIFORM_MATRIX:
        uniform.size = uniform.components * uniform.components * uniform.count * sizeof(int);
        uniform.value.data = calloc(1, uniform.size);
        break;

      case UNIFORM_SAMPLER:
        uniform.size = uniform.components * uniform.count * MAX(sizeof(Texture*), sizeof(int));
        uniform.value.data = calloc(1, uniform.size);

        // Use the value for ints to bind texture slots, but use the value for textures afterwards.
        for (int i = 0; i < uniform.count; i++) {
          uniform.value.ints[i] = uniform.baseTextureSlot + i;
        }
        glUniform1iv(uniform.location, uniform.count, uniform.value.ints);
        break;
    }

    size_t offset = 0;
    for (int j = 0; j < uniform.count; j++) {
      int location = uniform.location;

      if (uniform.count > 1) {
        char name[LOVR_MAX_UNIFORM_LENGTH];
        snprintf(name, LOVR_MAX_UNIFORM_LENGTH, "%s[%d]", uniform.name, j);
        location = glGetUniformLocation(program, name);
      }

      switch (uniform.type) {
        case UNIFORM_FLOAT:
          glGetUniformfv(program, location, &uniform.value.floats[offset]);
          offset += uniform.components;
          break;

        case UNIFORM_INT:
          glGetUniformiv(program, location, &uniform.value.ints[offset]);
          offset += uniform.components;
          break;

        case UNIFORM_MATRIX:
          glGetUniformfv(program, location, &uniform.value.floats[offset]);
          offset += uniform.components * uniform.components;
          break;

        default:
          break;
      }
    }

    map_set(&shader->uniforms, uniform.name, uniform);
    textureSlot += (uniform.type == UNIFORM_SAMPLER) ? uniform.count : 0;
  }

  // Attribute cache
  int32_t attributeCount;
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attributeCount);
  map_init(&shader->attributes);
  for (int i = 0; i < attributeCount; i++) {
    char name[LOVR_MAX_ATTRIBUTE_LENGTH];
    GLint size;
    GLenum type;
    glGetActiveAttrib(program, i, LOVR_MAX_ATTRIBUTE_LENGTH, NULL, &size, &type, name);
    map_set(&shader->attributes, name, glGetAttribLocation(program, name));
  }

  return shader;
}

Shader* lovrShaderCreateDefault(DefaultShader type) {
  switch (type) {
    case SHADER_DEFAULT: return lovrShaderCreate(NULL, NULL);
    case SHADER_CUBE: return lovrShaderCreate(lovrCubeVertexShader, lovrCubeFragmentShader); break;
    case SHADER_PANO: return lovrShaderCreate(lovrCubeVertexShader, lovrPanoFragmentShader); break;
    case SHADER_FONT: return lovrShaderCreate(NULL, lovrFontFragmentShader);
    case SHADER_FILL: return lovrShaderCreate(lovrFillVertexShader, NULL);
    default: lovrThrow("Unknown default shader type");
  }
}

void lovrShaderDestroy(void* ref) {
  Shader* shader = ref;
  glDeleteProgram(shader->program);
  map_deinit(&shader->uniforms);
  map_deinit(&shader->attributes);
  free(shader);
}

uint32_t lovrShaderGetProgram(Shader* shader) {
  return shader->program;
}

void lovrShaderBind(Shader* shader) {
  map_iter_t iter = map_iter(&shader->uniforms);
  const char* key;
  while ((key = map_next(&shader->uniforms, &iter)) != NULL) {
    Uniform* uniform = map_get(&shader->uniforms, key);

    if (uniform->type != UNIFORM_SAMPLER && !uniform->dirty) {
      continue;
    }

    uniform->dirty = false;
    int count = uniform->count;
    void* data = uniform->value.data;

    switch (uniform->type) {
      case UNIFORM_FLOAT:
        switch (uniform->components) {
          case 1: glUniform1fv(uniform->location, count, data); break;
          case 2: glUniform2fv(uniform->location, count, data); break;
          case 3: glUniform3fv(uniform->location, count, data); break;
          case 4: glUniform4fv(uniform->location, count, data); break;
        }
        break;

      case UNIFORM_INT:
        switch (uniform->components) {
          case 1: glUniform1iv(uniform->location, count, data); break;
          case 2: glUniform2iv(uniform->location, count, data); break;
          case 3: glUniform3iv(uniform->location, count, data); break;
          case 4: glUniform4iv(uniform->location, count, data); break;
        }
        break;

      case UNIFORM_MATRIX:
        switch (uniform->components) {
          case 2: glUniformMatrix2fv(uniform->location, count, GL_FALSE, data); break;
          case 3: glUniformMatrix3fv(uniform->location, count, GL_FALSE, data); break;
          case 4: glUniformMatrix4fv(uniform->location, count, GL_FALSE, data); break;
        }
        break;

      case UNIFORM_SAMPLER:
        for (int i = 0; i < count; i++) {
          GLenum uniformTextureType;
          switch (uniform->glType) {
            case GL_SAMPLER_2D: uniformTextureType = GL_TEXTURE_2D; break;
            case GL_SAMPLER_3D: uniformTextureType = GL_TEXTURE_3D; break;
            case GL_SAMPLER_CUBE: uniformTextureType = GL_TEXTURE_CUBE_MAP; break;
            case GL_SAMPLER_2D_ARRAY: uniformTextureType = GL_TEXTURE_2D_ARRAY; break;
          }
          gpuBindTexture(uniform->value.textures[i], uniform->baseTextureSlot + i);
        }
        break;
    }
  }
}

int lovrShaderGetAttributeId(Shader* shader, const char* name) {
  int* id = map_get(&shader->attributes, name);
  return id ? *id : -1;
}

bool lovrShaderHasUniform(Shader* shader, const char* name) {
  return map_get(&shader->uniforms, name) != NULL;
}

bool lovrShaderGetUniform(Shader* shader, const char* name, int* count, int* components, size_t* size, UniformType* type) {
  Uniform* uniform = map_get(&shader->uniforms, name);
  if (!uniform) {
    return false;
  }

  *count = uniform->count;
  *components = uniform->components;
  *size = uniform->size;
  *type = uniform->type;
  return true;
}

static void lovrShaderSetUniform(Shader* shader, const char* name, UniformType type, void* data, int count, size_t size, const char* debug) {
  Uniform* uniform = map_get(&shader->uniforms, name);
  if (!uniform) {
    return;
  }

  const char* plural = (uniform->size / size) > 1 ? "s" : "";
  lovrAssert(uniform->type == type, "Unable to send %ss to uniform %s", debug, uniform->name);
  lovrAssert(count * size <= uniform->size, "Expected at most %d %s%s for uniform %s, got %d", uniform->size / size, debug, plural, uniform->name, count);

  if (!uniform->dirty && !memcmp(uniform->value.data, data, count * size)) {
    return;
  }

  memcpy(uniform->value.data, data, count * size);
  uniform->dirty = true;
}

void lovrShaderSetFloat(Shader* shader, const char* name, float* data, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_FLOAT, data, count, sizeof(float), "float");
}

void lovrShaderSetInt(Shader* shader, const char* name, int* data, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_INT, data, count, sizeof(int), "int");
}

void lovrShaderSetMatrix(Shader* shader, const char* name, float* data, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_MATRIX, data, count, sizeof(float), "float");
}

void lovrShaderSetTexture(Shader* shader, const char* name, Texture** data, int count) {
  lovrShaderSetUniform(shader, name, UNIFORM_SAMPLER, data, count, sizeof(Texture*), "texture");
}
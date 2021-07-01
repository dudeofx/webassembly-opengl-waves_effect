#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>
#include <malloc.h>
#include <stdlib.h> 

// 20 vertices per row, 5 floats per vertex, 8 rows
#define MESH_FLOAT_TOTAL (20*5*8)

// point grid is 9x9 or 81 points, 2 floats per point
#define NUM_POINTS (81)
#define POINT_FLOAT_TOTAL (NUM_POINTS*2)

GLfloat triangle_points[POINT_FLOAT_TOTAL];
GLfloat point_offsets[NUM_POINTS];
GLfloat triangle_mesh[MESH_FLOAT_TOTAL];

const GLchar *vertex_shader_source = 
	 "attribute vec2  aVertex;"
	 "attribute vec2  aTexel;"
   "attribute float aOffset;"

   "uniform float uAngle;"

	 "varying vec2 vTexel;"

   "void main() {"
	 "   vTexel = aTexel;"
   "   float x_pos = aVertex.x+cos( (uAngle+aOffset)*0.0174533 )/32.0;"
   "   float y_pos = aVertex.y+sin( (uAngle+aOffset)*0.0174533 )/32.0;"
	 "   gl_Position = vec4(x_pos, y_pos, 0.0, 1.1);"
	 "}";
		
const GLchar *fragment_shader_source =
	 "precision mediump float;"

	 "varying vec2 vTexel;"

	 "uniform sampler2D uTexmap;"
   "uniform float uShift_s;"
   "uniform float uShift_t;"

	 "void main() {"
   "   float s = vTexel.s + uShift_s;"
   "   float t = vTexel.t + uShift_t;"
	 "   gl_FragColor = texture2D(uTexmap, vec2(s, t));"
	 "}";

   GLuint   vbo;
   GLint    handle_aVertex;
   GLint    handle_aTexel;
   GLint    handle_aOffset;
   GLint    handle_uTexmap;
   GLint    handle_uShift_s;
   GLint    handle_uShift_t;
   GLint    handle_uAngle;
   GLuint   texture_handle[1];

//-------------------------------------------------------------------------------
void OnDrawFrame() {
   static int frame_count = 0;
   float angle = frame_count / 0.5f;

   frame_count++;

   glUniform1f(handle_uAngle, angle);

   // change textures every 384 frames
   glUniform1f(handle_uShift_s, ((frame_count / 384) & 1) / 2.0f );
   glUniform1f(handle_uShift_t, ((frame_count / 384) / 2) / 2.0f );

   glDrawArrays(GL_TRIANGLE_STRIP, 0, 160);
}
//-------------------------------------------------------------------------------
void LoadTexture() {
   int w, h;
   char *img_data;

   img_data = emscripten_get_preloaded_image_data("assets/texture_sheet00.jpg", &w, &h);
   glGenTextures(1, texture_handle);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, texture_handle[0]);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img_data);
   free(img_data);

}
//-------------------------------------------------------------------------------
void SetupPoints() {
   for (int i = 0; i < 9; i++) {
      for (int j = 0; j < 9; j++) {
         triangle_points[(i*9+j)*2+0] = (j/4.0f)-1.0f; // x coord
         triangle_points[(i*9+j)*2+1] = -1*((i/4.0f)-1.0f); // y coord
         point_offsets[i*9+j] = ((float) (rand() % 360));
      }
   }
}
//-------------------------------------------------------------------------------
void SetupMesh() {
   // init mesh
   int inx = 0;
   int stride = 9*2; // int the 9x9 grid of points, strid is how many floats total per row
   for (int row = 0; row < 8; row++) {
      int i = row*stride; // (i) will start at the upper left point of the 9x9 grid

      // 1st leading point in strip
      triangle_mesh[inx++] = triangle_points[i+0]; // x coord
      triangle_mesh[inx++] = triangle_points[i+1]; // y coord
      inx += 2; // reserve the next 2 floats for texture
      triangle_mesh[inx++] = point_offsets[row*9];

      // 2nd leading point in strip
      triangle_mesh[inx++] = triangle_points[i+stride+0]; // x coord
      triangle_mesh[inx++] = triangle_points[i+stride+1]; // y coord
      inx += 2; // reserve the next 2 floats for texture
      triangle_mesh[inx++] = point_offsets[(row+1)*9];
   
      for (int col = 0; col < 8; col++) {
         i += 2; // bump (i) to the next point

         // close the upper triangle on cell
         triangle_mesh[inx++] = triangle_points[i+0]; // x coord
         triangle_mesh[inx++] = triangle_points[i+1]; // y coord
         inx += 2; // reserve the next 2 floats for texture
         triangle_mesh[inx++] = point_offsets[row*9+col+1];

         // close the bottom triangle on cell
         triangle_mesh[inx++] = triangle_points[i+stride+0]; // x coord
         triangle_mesh[inx++] = triangle_points[i+stride+1]; // y coord
         inx += 2; // reserve the next 2 floats for texture
         triangle_mesh[inx++] = point_offsets[(row+1)*9+col+1];
      }

      // 1st degenerate triangle is done by repeating the last point (i) didn't change;
      triangle_mesh[inx++] = triangle_points[i+stride+0]; // x coord
      triangle_mesh[inx++] = triangle_points[i+stride+1]; // y coord
      inx += 2; // reserve the next 2 floats for texture
      triangle_mesh[inx++] = point_offsets[(row+1)*9+8];

      // 2nd degenerate triangle is done by repeating the 1st point of the leading point of the next strip
      triangle_mesh[inx++] = triangle_points[(row+1)*stride+0]; // x coord
      triangle_mesh[inx++] = triangle_points[(row+1)*stride+1]; // y coord
      inx += 2; // reserve the next 2 floats for texture
      triangle_mesh[inx++] = point_offsets[(row+1)*9];

      // the next cycle will automatically create 2 degenerate triangles using the leading points
   }
}
//-------------------------------------------------------------------------------
void SetupTexels() {
   float u, v;
   int i = 0;
   while (i < MESH_FLOAT_TOTAL) { 
      i += 2;
      u = triangle_mesh[i-2]/2.0f+0.5; 
      triangle_mesh[i++] = u/2.0f;
      v = 1.0 - (triangle_mesh[i-2]/2.0f+0.5); 
      triangle_mesh[i++] = v/2.0f;
      i++;
   }
}
//-------------------------------------------------------------------------------

int main() {

   EmscriptenWebGLContextAttributes attr;
   emscripten_webgl_init_context_attributes(&attr);

   // a context is required to use the WebGL API
   EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#my_canvas", &attr);

   // binds the WebGL API to the context
   emscripten_webgl_make_context_current(ctx);
   emscripten_set_canvas_element_size("#my_canvas", 640, 640);
   glViewport(0, 0, 640, 640);

   SetupPoints();
   SetupMesh();
   SetupTexels();

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   glGenBuffers(1, &vbo);

   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_mesh), triangle_mesh, GL_STATIC_DRAW);

   GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
   glCompileShader(vertex_shader);

   GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
   glCompileShader(fragment_shader);

   GLuint shader_program = glCreateProgram();
   glAttachShader(shader_program, vertex_shader);
   glAttachShader(shader_program, fragment_shader);
   glLinkProgram(shader_program);
   glUseProgram(shader_program); 

   handle_aVertex = glGetAttribLocation(shader_program, "aVertex");
   handle_aTexel = glGetAttribLocation(shader_program, "aTexel");
   handle_aOffset = glGetAttribLocation(shader_program, "aOffset");
   handle_uTexmap = glGetUniformLocation(shader_program, "uTexmap");
   handle_uAngle = glGetUniformLocation(shader_program, "uAngle");
   handle_uShift_s = glGetUniformLocation(shader_program, "uShift_s");
   handle_uShift_t = glGetUniformLocation(shader_program, "uShift_t");

   LoadTexture();

   glVertexAttribPointer(handle_aVertex, 2, GL_FLOAT, GL_FALSE, sizeof(float)*5, 0);
   glEnableVertexAttribArray(handle_aVertex);   

   glVertexAttribPointer(handle_aTexel, 2, GL_FLOAT, GL_FALSE, sizeof(float)*5, (void *)(2*sizeof(float)));
   glEnableVertexAttribArray(handle_aTexel);   

   glVertexAttribPointer(handle_aOffset, 1, GL_FLOAT, GL_FALSE, sizeof(float)*5, (void *)(4*sizeof(float)));
   glEnableVertexAttribArray(handle_aOffset);   

   glUniform1i(handle_uTexmap, 0);

   emscripten_set_main_loop(OnDrawFrame, 0, 1);

   return 0;
}
//-------------------------------------------------------------------------------


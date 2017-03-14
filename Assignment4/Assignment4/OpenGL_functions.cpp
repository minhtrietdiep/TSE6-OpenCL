#include <GL\freeglut.h>

GLuint init_gl(int tex_width, int tex_height)
{
	GLuint g_texture = 0;
	
	// Initialize texture
	glGenTextures(1, &g_texture);
	glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_width, tex_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glEnable(GL_TEXTURE_2D);
    
    // Initialize viewport
	glViewport(0, 0, 800, 800);

    // Setup camera
    glMatrixMode( GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

    // No lighting or depth testing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    return g_texture;
}

void draw_quad()
{
	// Define the points of the quad, together with texture coordinates
	glBegin(GL_QUADS);
      glTexCoord2f(0.0f, 0.0f);
      glVertex3f(-1.0f, -1.0f, 0.1f);

      glTexCoord2f(1.0f, 0.0f);
      glVertex3f(1.0f, -1.0f, 0.1f);

      glTexCoord2f(1.0f, 1.0f);
      glVertex3f(1.0f, 1.0f, 0.1f);

      glTexCoord2f(0.0f, 1.0f);
     glVertex3f(-1.0f, 1.0f, 0.1f);
    glEnd();
}
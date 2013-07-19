#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/glu.h>
#include <GL/gl.h>
#include "imageloader.h"
#include "vec3f.h"
#endif



static GLfloat spin, spin2 = 0.0;
float angle = 0;
using namespace std;

float lastx, lasty;
GLint stencilBits;
static int viewx = 130;
static int viewy = 110;
static int viewz = 200;

float rot = 0;

GLuint texture[1]; //array untuk texture

struct Images {//tempat image
	unsigned long sizeX;
	unsigned long sizeY;
	char *data;
};
typedef struct Images Images;

//train 2D
//class untuk terain 2D
class Terrain {
private:
	int w; //Widt
	int l; //Length
	float** hs; //Heights
	Vec3f** normals;
	bool computedNormals; //Whether normals is up-to-date
public:
	Terrain(int w2, int l2) {
		w = w2;
		l = l2;

		hs = new float*[l];
		for (int i = 0; i < l; i++) {
			hs[i] = new float[w];
		}

		normals = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals[i] = new Vec3f[w];
		}

		computedNormals = false;
	}

	~Terrain() {
		for (int i = 0; i < l; i++) {
			delete[] hs[i];
		}
		delete[] hs;

		for (int i = 0; i < l; i++) {
			delete[] normals[i];
		}
		delete[] normals;
	}

	int width() {
		return w;
	}

	int length() {
		return l;
	}

	//Sets the height at (x, z) to y
	void setHeight(int x, int z, float y) {
		hs[z][x] = y;
		computedNormals = false;
	}

	//Returns the height at (x, z)
	float getHeight(int x, int z) {
		return hs[z][x];
	}

	//Computes the normals, if they haven't been computed yet
	void computeNormals() {
		if (computedNormals) {
			return;
		}

		//Compute the rough version of the normals
		Vec3f** normals2 = new Vec3f*[l];
		for (int i = 0; i < l; i++) {
			normals2[i] = new Vec3f[w];
		}

		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum(0.0f, 0.0f, 0.0f);

				Vec3f out;
				if (z > 0) {
					out = Vec3f(0.0f, hs[z - 1][x] - hs[z][x], -1.0f);
				}
				Vec3f in;
				if (z < l - 1) {
					in = Vec3f(0.0f, hs[z + 1][x] - hs[z][x], 1.0f);
				}
				Vec3f left;
				if (x > 0) {
					left = Vec3f(-1.0f, hs[z][x - 1] - hs[z][x], 0.0f);
				}
				Vec3f right;
				if (x < w - 1) {
					right = Vec3f(1.0f, hs[z][x + 1] - hs[z][x], 0.0f);
				}

				if (x > 0 && z > 0) {
					sum += out.cross(left).normalize();
				}
				if (x > 0 && z < l - 1) {
					sum += left.cross(in).normalize();
				}
				if (x < w - 1 && z < l - 1) {
					sum += in.cross(right).normalize();
				}
				if (x < w - 1 && z > 0) {
					sum += right.cross(out).normalize();
				}

				normals2[z][x] = sum;
			}
		}

		//Smooth out the normals
		const float FALLOUT_RATIO = 0.5f;
		for (int z = 0; z < l; z++) {
			for (int x = 0; x < w; x++) {
				Vec3f sum = normals2[z][x];

				if (x > 0) {
					sum += normals2[z][x - 1] * FALLOUT_RATIO;
				}
				if (x < w - 1) {
					sum += normals2[z][x + 1] * FALLOUT_RATIO;
				}
				if (z > 0) {
					sum += normals2[z - 1][x] * FALLOUT_RATIO;
				}
				if (z < l - 1) {
					sum += normals2[z + 1][x] * FALLOUT_RATIO;
				}

				if (sum.magnitude() == 0) {
					sum = Vec3f(0.0f, 1.0f, 0.0f);
				}
				normals[z][x] = sum;
			}
		}

		for (int i = 0; i < l; i++) {
			delete[] normals2[i];
		}
		delete[] normals2;

		computedNormals = true;
	}

	//Returns the normal at (x, z)
	Vec3f getNormal(int x, int z) {
		if (!computedNormals) {
			computeNormals();
		}
		return normals[z][x];
	}
};
//end class


void initRendering() {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
	glShadeModel(GL_SMOOTH);
}

//Loads a terrain from a heightmap.  The heights of the terrain range from
//-height / 2 to height / 2.
//load terain di procedure inisialisasi
Terrain* loadTerrain(const char* filename, float height) {
	Image* image = loadBMP(filename);
	Terrain* t = new Terrain(image->width, image->height);
	for (int y = 0; y < image->height; y++) {
		for (int x = 0; x < image->width; x++) {
			unsigned char color = (unsigned char) image->pixels[3 * (y
					* image->width + x)];
			float h = height * ((color / 255.0f) - 0.5f);
			t->setHeight(x, y, h);
		}
	}

	delete image;
	t->computeNormals();
	return t;
}

float _angle = 60.0f;
//buat tipe data terain
Terrain* _terrain;
Terrain* _terrainTanah;
Terrain* _terrainAir;
Terrain* _terrainJalan;


const GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
const GLfloat light_diffuse[] = { 0.7f, 0.7f, 0.7f, 1.0f };
const GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat light_position[] = { 1.0f, 1.0f, 1.0f, 1.0f };

const GLfloat light_ambient2[] = { 0.3f, 0.3f, 0.3f, 0.0f };
const GLfloat light_diffuse2[] = { 0.3f, 0.3f, 0.3f, 0.0f };

const GLfloat mat_ambient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
const GLfloat mat_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
const GLfloat high_shininess[] = { 100.0f };

void cleanup() {
	delete _terrain;
	delete _terrainTanah;
	delete _terrainAir;
	delete _terrainJalan;
}

//mengambil gambar BMP
int ImageLoad(char *filename, Images *image) {
	FILE *file;
	unsigned long size; // ukuran image dalam bytes
	unsigned long i; // standard counter.
	unsigned short int plane; // number of planes in image

	unsigned short int bpp; // jumlah bits per pixel
	char temp; // temporary color storage for var warna sementara untuk memastikan filenya ada


	if ((file = fopen(filename, "rb")) == NULL) {
		printf("File Not Found : %s\n", filename);
		return 0;
	}
	// mencari file header bmp
	fseek(file, 18, SEEK_CUR);
	// read the width
	if ((i = fread(&image->sizeX, 4, 1, file)) != 1) {//lebar beda
		printf("Error reading width from %s.\n", filename);
		return 0;
	}
	//printf("Width of %s: %lu\n", filename, image->sizeX);
	// membaca nilai height
	if ((i = fread(&image->sizeY, 4, 1, file)) != 1) {//tingginya beda
		printf("Error reading height from %s.\n", filename);
		return 0;
	}
	//printf("Height of %s: %lu\n", filename, image->sizeY);
	//menghitung ukuran image(asumsi 24 bits or 3 bytes per pixel).

	size = image->sizeX * image->sizeY * 3;
	// read the planes
	if ((fread(&plane, 2, 1, file)) != 1) {
		printf("Error reading planes from %s.\n", filename);//bukan file bmp
		return 0;
	}
	if (plane != 1) {
		printf("Planes from %s is not 1: %u\n", filename, plane);//
		return 0;
	}
	// read the bitsperpixel
	if ((i = fread(&bpp, 2, 1, file)) != 1) {
		printf("Error reading bpp from %s.\n", filename);

		return 0;
	}
	if (bpp != 24) {
		printf("Bpp from %s is not 24: %u\n", filename, bpp);//bukan 24 pixel
		return 0;
	}
	// seek past the rest of the bitmap header.
	fseek(file, 24, SEEK_CUR);
	// read the data.
	image->data = (char *) malloc(size);
	if (image->data == NULL) {
		printf("Error allocating memory for color-corrected image data");//gagal ambil memory
		return 0;
	}
	if ((i = fread(image->data, size, 1, file)) != 1) {
		printf("Error reading image data from %s.\n", filename);
		return 0;
	}
	for (i = 0; i < size; i += 3) { // membalikan semuan nilai warna (gbr - > rgb)
		temp = image->data[i];
		image->data[i] = image->data[i + 2];
		image->data[i + 2] = temp;
	}
	// we're done.
	return 1;
}




//mengambil tekstur
Images * loadTexture() {
	Images *image1;
	// alokasi memmory untuk tekstur
	image1 = (Images *) malloc(sizeof(Images));
	if (image1 == NULL) {
		printf("Error allocating space for image");//memory tidak cukup
		exit(0);
	}
	//pic.bmp is a 64x64 picture
	if (!ImageLoad("Wood-10777.bmp", image1)) {
		exit(1);
	}
	return image1;
}





//untuk di display
void drawSceneTanah(Terrain *terrain, GLfloat r, GLfloat g, GLfloat b) {
	
	float scale = 500.0f / max(terrain->width() - 1, terrain->length() - 1);
	glScalef(scale, scale, scale);
	glTranslatef(-(float) (terrain->width() - 1) / 2, 0.0f,
			-(float) (terrain->length() - 1) / 2);

	glColor3f(r, g, b);
	for (int z = 0; z < terrain->length() - 1; z++) {
		//Makes OpenGL draw a triangle at every three consecutive vertices
		glBegin(GL_TRIANGLE_STRIP);
		for (int x = 0; x < terrain->width(); x++) {
			Vec3f normal = terrain->getNormal(x, z);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z), z);
			normal = terrain->getNormal(x, z + 1);
			glNormal3f(normal[0], normal[1], normal[2]);
			glVertex3f(x, terrain->getHeight(x, z + 1), z + 1);
		}
		glEnd();
	}

}

GLuint loadtextures(const char *filename, int width, int height) {//buat ngambil dari file image untuk jadi texture
	GLuint texture;

	unsigned char *data;
	FILE *file;


	file = fopen(filename, "rb");
	if (file == NULL)
		return 0;

	data = (unsigned char *) malloc(width * height * 3);  //file pendukung texture
	fread(data, width * height * 3, 1, file);

	fclose(file);

	glGenTextures(1, &texture);//generet (dibentuk)
	glBindTexture(GL_TEXTURE_2D, texture);//binding (gabung)
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_LINEAR_MIPMAP_NEAREST);//untuk membaca gambar jadi text dan dapat dibaca dengan pixel
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameterf(  GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, width, height, GL_RGB,
			GL_UNSIGNED_BYTE, data);

	data = NULL;

	return texture;
}






























void kereta(){
   //badan kereta 1
   glBegin(GL_POLYGON); 
        glColor3f(0,1,0);
      	glVertex3f(10,-4,2);
     	glVertex3f(10,3,2);
        glVertex3f(24,3,2);
        glVertex3f(24,-4,2);
  	glEnd();
  	
   //bg belakang kereta 1
    glBegin(GL_POLYGON);
  	  glColor3f(0,1,0);   
    	glVertex3f(10,-4, -4);
        glVertex3f(10,3, -4);
        glVertex3f(24,3, -4);
        glVertex3f(24,-4, -4);
  	 glEnd();
  	 
  	 //badan kereta 1 samping kiri
        glBegin(GL_POLYGON); 
        glColor3f(0,0.8,0); 
      	glVertex3f(10,-4,2);
     	glVertex3f(10,-4, -4);
        glVertex3f(10,3, -4);
        glVertex3f(10,3,2);
  	glEnd();
  	 
  	 //badan kereta 1 samping kanan
        glBegin(GL_POLYGON); 
        glColor3f(0,0.8,0); 
      	glVertex3f(24,-4,2);
     	glVertex3f(24,-4, -4);
        glVertex3f(24,3, -4);
        glVertex3f(24,3,2);
  	glEnd(); 
  	
  	//badan kereta 1 bawah
        glBegin(GL_POLYGON); 
        glColor3f(0,0.7,0); 
      	glVertex3f(10,-4,2);
     	glVertex3f(10,-4, -4);
        glVertex3f(24,-4, -4);
        glVertex3f(24,-4,2);
  	glEnd(); 
  	
  	//badan kereta 1 atas
        glBegin(GL_POLYGON); 
        glColor3f(0,0.7,0); 
      	glVertex3f(10,3,-4);
     	glVertex3f(10,3,2);
        glVertex3f(24,3,2);
        glVertex3f(24,3,-4);
  	glEnd(); 
  	 
   //badan kereta 2
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.0,0.0); 
      	glVertex3f(-2,-4,2);
     	glVertex3f(-2,3,2);
        glVertex3f(9,3,2);
        glVertex3f(9,-4,2);
  	glEnd();
  	
  	//atap depan kereta 2
    glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-2.25,4,2);
     	glVertex3f(-2.25,3,2);
        glVertex3f(9.25,3,2);
        glVertex3f(9.25,4,2);
  	glEnd();
  	
  	//atap belakang kereta 2
    glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-2.25,4,-4);
     	glVertex3f(-2.25,3,-4);
        glVertex3f(9.25,3,-4);
        glVertex3f(9.25,4,-4);
  	glEnd();

   //atap kereta 2 samping kiri
    glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-2.25,4,2.1);
     	glVertex3f(-2.25,4,-4.1);
        glVertex3f(-2.25,3,-4.1);
        glVertex3f(-2.25,3,2.1);
  	glEnd();

  	 //atap kereta 2 samping kanan
    glBegin(GL_POLYGON);
  	  glColor3f(0.0,0.0,0.0);   
    	glVertex3f(9.25,4,2.1);
        glVertex3f(9.25,4,-4.1);
        glVertex3f(9.25,3,-4.1);
        glVertex3f(9.25,3,2.1);
  	 glEnd(); 
  	 
  	 //atap kereta 2 atas
    glBegin(GL_POLYGON);
  	  glColor3f(0.2,0.2,0.2);   
    	glVertex3f(-2.25,4,-4);
        glVertex3f(-2.25,4,2);
        glVertex3f(9.25,4,2);
        glVertex3f(9.25,4,-4);
  	 glEnd();

   //jendela1 depan kereta 2
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(-1,-1,2.1);
     	glVertex3f(-1,2.5,2.1);
        glVertex3f(3,2.5,2.1);
        glVertex3f(3,-1,2.1);
  	glEnd();
  	
   //jendela2 depan kereta 2
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(4,-1,2.1);
     	glVertex3f(4,2.5,2.1);
        glVertex3f(8,2.5,2.1);
        glVertex3f(8,-1,2.1);
  	glEnd();
  	
    //jendela1 belakang kereta 2
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(3,-1,-4.1);
     	glVertex3f(3,2.5,-4.1);
        glVertex3f(-1,2.5,-4.1);
        glVertex3f(-1,-1,-4.1);
  	glEnd();
  	
  	//jendela2 belakang kereta 2
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(4,-1,-4.1);
     	glVertex3f(4,2.5,-4.1);
        glVertex3f(8,2.5,-4.1);
        glVertex3f(8,-1,-4.1);
  	glEnd();
  		
 	  //bg belakang kereta 2
    glBegin(GL_POLYGON);
  	  glColor3f(1.0,0.0,0.0);   
    	glVertex3f(-2,-4, -4);
        glVertex3f(-2,3, -4);
        glVertex3f(9,3, -4);
        glVertex3f(9,-4, -4);
  	 glEnd();
  	
  	//badan kereta 2 samping kiri
    glBegin(GL_POLYGON);
  	  glColor3f(0.8,0.0,0.0);   
    	glVertex3f(-2,-4,2);
        glVertex3f(-2,-4,-4);
        glVertex3f(-2,3,-4);
        glVertex3f(-2,3,2);
  	 glEnd();  
  	 
  	 //badan kereta 2 samping kanan
    glBegin(GL_POLYGON);
  	  glColor3f(0.8,0.0,0.0);   
    	glVertex3f(9,-4,2);
        glVertex3f(9,-4,-4);
        glVertex3f(9,3,-4);
        glVertex3f(9,3,2);
  	 glEnd();  
  	 
  	 //badan kereta 2 bawah
    glBegin(GL_POLYGON);
  	  glColor3f(0.7,0.0,0.0);   
    	glVertex3f(-2,-4,2);
        glVertex3f(-2,-4,-4);
        glVertex3f(9,-4,-4);
        glVertex3f(9,-4,2);
  	 glEnd(); 
  	 
  	 //badan kereta 2 atas
    glBegin(GL_POLYGON);
  	  glColor3f(0.7,0.0,0.0);   
    	glVertex3f(-2.25,3,-4);
        glVertex3f(-2.25,3,2);
        glVertex3f(9.25,3,2);
        glVertex3f(9.25,3,-4);
  	 glEnd(); 
  	 
   //badan kereta 3
   glBegin(GL_POLYGON); 
        glColor3f(0.0,1.0,0.0); 
      	glVertex3f(-14,-4,2);
     	glVertex3f(-14,3,2);
        glVertex3f(-3,3,2);
        glVertex3f(-3,-4,2);
  	glEnd();
  	
  	//atap depan kereta 3
    glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-14.25,4,2);
     	glVertex3f(-14.25,3,2);
        glVertex3f(-2.75,3,2);
        glVertex3f(-2.75,4,2);
  	glEnd();

  	//atap belakang kereta 3
    glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-14.25,4,-4);
     	glVertex3f(-14.25,3,-4);
        glVertex3f(-2.75,3,-4);
        glVertex3f(-2.75,4,-4);
  	glEnd();

  	 //atap kereta 3 samping kiri
    glBegin(GL_POLYGON);
  	  glColor3f(0.0,0.0,0.0);   
    	glVertex3f(-14.25,4,2.1);
        glVertex3f(-14.25,4,-4.1);
        glVertex3f(-14.25,3,-4.1);
        glVertex3f(-14.25,3,2.1);
  	 glEnd(); 
  	 
  	//badan kereta 3 atas
   glBegin(GL_POLYGON); 
        glColor3f(0.2,0.2,0.2); 
      	glVertex3f(-14.25,4, -4);
     	glVertex3f(-14.25,4,2);
        glVertex3f(-2.75,4,2);
        glVertex3f(-2.75,4, -4);
  	glEnd();
  	
  	//atap kereta 3 samping kanan
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.8,0.0); 
      	glVertex3f(-3,-4,2);
     	glVertex3f(-3,-4, -4);
        glVertex3f(-3,3, -4);
        glVertex3f(-3,3,2);
  	glEnd();

   //jendela1 depan 3 kereta 3
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(-13,-1,2.1);
     	glVertex3f(-13,2.5,2.1);
        glVertex3f(-9,2.5,2.1);
        glVertex3f(-9,-1,2.1);
  	glEnd();

  	 //jendela2 depan 3 kereta 3
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(-8,-1,2.1);
     	glVertex3f(-8,2.5,2.1);
        glVertex3f(-4,2.5,2.1);
        glVertex3f(-4,-1,2.1);
  	glEnd();
  	
  	 //jendela1 belakang kereta 3
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(-13,-1,-4.1);
     	glVertex3f(-13,2.5,-4.1);
        glVertex3f(-9,2.5,-4.1);
        glVertex3f(-9,-1,-4.1);
  	glEnd();
  	
  	//jendela2 belakang kereta 3
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(-4,-1,-4.1);
     	glVertex3f(-4,2.5,-4.1);
        glVertex3f(-8,2.5,-4.1);
        glVertex3f(-8,-1,-4.1);
  	glEnd();
  	
  	  //bg belakang kereta 3
    glBegin(GL_POLYGON);
  	  glColor3f(0.0,1.0,0.0);   
    	glVertex3f(-3,-4, -4);
        glVertex3f(-3,3, -4);
        glVertex3f(-14,3, -4);
        glVertex3f(-14,-4, -4);
  	 glEnd();
  	 
  	 //badan kereta 3 samping kiri
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.8,0.0); 
      	glVertex3f(-14,-4,2);
     	glVertex3f(-14,-4, -4);
        glVertex3f(-14,3, -4);
        glVertex3f(-14,3,2);
  	glEnd();
  	
  	//badan kereta 3 samping kanan
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.8,0.0); 
      	glVertex3f(-3,-4,2);
     	glVertex3f(-3,-4, -4);
        glVertex3f(-3,3, -4);
        glVertex3f(-3,3,2);
  	glEnd();
  	
  	//badan kereta 3 bawah
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.7,0.0); 
      	glVertex3f(-14,-4,2);
     	glVertex3f(-14,-4, -4);
        glVertex3f(-3,-4, -4);
        glVertex3f(-3,-4,2);
  	glEnd();
  	
  	//badan kereta 3 atas
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.7,0.0); 
      	glVertex3f(-14.25,3, -4);
     	glVertex3f(-14.25,3,2);
        glVertex3f(-2.75,3,2);
        glVertex3f(-2.75,3, -4);
  	glEnd();
  	
   //badan kereta 4
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.6,0.0); 
      	glVertex3f(-26,-4,2);
     	glVertex3f(-26,3,2);
        glVertex3f(-15,3,2);
        glVertex3f(-15,-4,2);
  	glEnd();
  	
  	//atap depan kereta 4
    glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-26.25,4,2);
     	glVertex3f(-26.25,3,2);
        glVertex3f(-14.75,3,2);
        glVertex3f(-14.75,4,2);
  	glEnd();

  	//atap belakang kereta 4
    glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-26.25,4,-4);
     	glVertex3f(-26.25,3,-4);
        glVertex3f(-14.75,3,-4);
        glVertex3f(-14.75,4,-4);
  	glEnd();
	
  	//atap kereta 4 samping kiri
    glBegin(GL_POLYGON);
  	  glColor3f(0.0,0.0,0.0);   
    	glVertex3f(-26.25,4,2);
        glVertex3f(-26.25,4, -4);
        glVertex3f(-26.25,3, -4);
        glVertex3f(-26.25,3, 2);
  	 glEnd();
  	
  	//atap kereta 4 samping kanan
    glBegin(GL_POLYGON);
  	  glColor3f(0.0,0.0,0.0);   
    	glVertex3f(-14.75,4,2);
        glVertex3f(-14.75,4, -4);
        glVertex3f(-14.75,3, -4);
        glVertex3f(-14.75,3, 2);
  	 glEnd();
  	 
     //atap kereta 4 atas
    glBegin(GL_POLYGON);
  	  glColor3f(0.2,0.2,0.2);   
    	glVertex3f(-26.25,4, -4);
        glVertex3f(-26.25,4, 2);
        glVertex3f(-14.75,4, 2);
        glVertex3f(-14.75,4, -4);
  	 glEnd();

   //jendela depan 1 kereta 4
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(-25,-1,2.1);
     	glVertex3f(-25,2.5,2.1);
        glVertex3f(-21,2.5,2.1);
        glVertex3f(-21,-1,2.1);
  	glEnd();
 
   //jendela depan 2 kereta 4
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(-20,-1,2.1);
     	glVertex3f(-20,2.5,2.1);
        glVertex3f(-16,2.5,2.1);
        glVertex3f(-16,-1,2.1);
  	glEnd();
  	
   //jendela belakang 1 kereta 4
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(-21,-1,-4.1);
     	glVertex3f(-21,2.5,-4.1);
        glVertex3f(-25,2.5,-4.1);
        glVertex3f(-25,-1,-4.1);
  	glEnd();
  	
   //jendela belakang 2 kereta 4
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(-16,-1,-4.1);
     	glVertex3f(-16,2.5,-4.1);
        glVertex3f(-20,2.5,-4.1);
        glVertex3f(-20,-1,-4.1);
  	glEnd();
  	
   //bg belakang kereta 4
    glBegin(GL_POLYGON);
  	  glColor3f(1.0,0.6,0.0);   
    	glVertex3f(-15,-4, -4);
        glVertex3f(-15,3, -4);
        glVertex3f(-26,3, -4);
        glVertex3f(-26,-4, -4);
  	 glEnd();
  	
  	//badan kereta 4 samping kiri
    glBegin(GL_POLYGON);
  	  glColor3f(1.0,0.4,0.0);   
    	glVertex3f(-26,-4,2);
        glVertex3f(-26,-4, -4);
        glVertex3f(-26,3, -4);
        glVertex3f(-26,3, 2);
  	 glEnd();
  	
  	//badan kereta 4 samping kanan
    glBegin(GL_POLYGON);
  	  glColor3f(1.0,0.4,0.0);   
    	glVertex3f(-15,-4,2);
        glVertex3f(-15,-4, -4);
        glVertex3f(-15,3, -4);
        glVertex3f(-15,3, 2);
  	 glEnd();
  	 
  	 //badan kereta 4 bawah
    glBegin(GL_POLYGON);
  	  glColor3f(1.0,0.3,0.0);   
    	glVertex3f(-26,-4,2);
        glVertex3f(-26,-4, -4);
        glVertex3f(-15,-4, -4);
        glVertex3f(-15,-4,2);
  	 glEnd();
  	 
  	  //badan kereta 4 atas
    glBegin(GL_POLYGON);
  	  glColor3f(1.0,0.3,0.0);   
    	glVertex3f(-26.25,3, -4);
        glVertex3f(-26.25,3, 2);
        glVertex3f(-14.75,3, 2);
        glVertex3f(-14.75,3, -4);
  	 glEnd();
  	
   //sambungan kereta 1
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(7,-4,1.9);
     	glVertex3f(7,-3,1.9);
        glVertex3f(11,-3,1.9);
        glVertex3f(11,-4,1.9);
  	glEnd();
  	
   //sambungan kereta 2
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-5,-4,1.9);
     	glVertex3f(-5,-3,1.9);
        glVertex3f(-1,-3,1.9);
        glVertex3f(-1,-4,1.9);
  	glEnd();
  	
  	//sambungan kereta 3
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-17,-4,1.9);
     	glVertex3f(-17,-3,1.9);
        glVertex3f(-13,-3,1.9);
        glVertex3f(-13,-4,1.9);
  	glEnd();
  	
  	 //sambungan belakang kereta 1
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(7,-4,-3.9);
     	glVertex3f(7,-3,-3.9);
        glVertex3f(11,-3,-3.9);
        glVertex3f(11,-4,-3.9);
  	glEnd();
  	
   //sambungan belakang kereta 2
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-5,-4,-3.9);
     	glVertex3f(-5,-3,-3.9);
        glVertex3f(-1,-3,-3.9);
        glVertex3f(-1,-4,-3.9);
  	glEnd();
  	
  	//sambungan belakang kereta 3
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(-17,-4,-3.9);
     	glVertex3f(-17,-3,-3.9);
        glVertex3f(-13,-3,-3.9);
        glVertex3f(-13,-4,-3.9);
  	glEnd();
  	
  	//depan badan 1 kereta depan
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.0,0.0); 
      	glVertex3f(24,-4,2);
     	glVertex3f(24,-2,2);
        glVertex3f(25,-2,2);
        glVertex3f(26,-4,2);
  	glEnd();
      
      //depan badan 1 kereta belakang
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.0,0.0); 
      	glVertex3f(26,-4,-4);
     	glVertex3f(25,-2,-4);
        glVertex3f(24,-2,-4);
        glVertex3f(24,-4,-4);
  	glEnd();
      
      //depan badan 1 kereta samping kiri
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.0,0.0); 
      	glVertex3f(24,-4,2);
     	glVertex3f(24,-2,-4);
        glVertex3f(24,-2,-4);
        glVertex3f(24,-4,2);
  	glEnd();	
  	
  	 //depan badan 1 kereta samping kanan
   glBegin(GL_POLYGON); 
        glColor3f(0.8,0.0,0.0); 
      	glVertex3f(26,-4,2);
     	glVertex3f(25,-2,2);
        glVertex3f(25,-2,-4);
        glVertex3f(26,-4,-4);
  	glEnd();
  	
  	 //depan badan 1 kereta bawah
   glBegin(GL_POLYGON); 
        glColor3f(0.7,0.0,0.0); 
      	glVertex3f(24,-4,2);
     	glVertex3f(24,-4,-4);
        glVertex3f(26,-4,-4);
        glVertex3f(26,-4,2);
  	glEnd();
  	
  	//depan badan 1 kereta atas
   glBegin(GL_POLYGON); 
        glColor3f(0.7,0.0,0.0); 
      	glVertex3f(24,-2,2);
     	glVertex3f(24,-2,-4);
        glVertex3f(25,-2,-4);
        glVertex3f(25,-2,2);
  	glEnd();
  	
   //corong depan
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,1.0); 
      	glVertex3f(20,3,0);
     	glVertex3f(19.5,5,0);
        glVertex3f(21.5,5,0);
        glVertex3f(21,3,0);
  	glEnd();
  	
  	//corong belakang
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,1.0); 
      	glVertex3f(20,3,-2);
     	glVertex3f(19.5,5,-2);
        glVertex3f(21.5,5,-2);
        glVertex3f(21,3,-2);
  	glEnd();
  	
  	//corong samping kiri
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.8); 
      	glVertex3f(20,3,0);
     	glVertex3f(20,3,-2);
        glVertex3f(19.5,5,-2);
        glVertex3f(19.5,5,0);
  	glEnd();
  	
  	//corong samping kanan
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.8); 
      	glVertex3f(21,3,0);
     	glVertex3f(21,3,-2);
        glVertex3f(21.5,5,-2);
        glVertex3f(21.5,5,0);
  	glEnd();
  	
  	//corong bawah
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,0.0); 
      	glVertex3f(20,3.2,0);
     	glVertex3f(20,3.2,-2);
        glVertex3f(21,3.2,-2);
        glVertex3f(21,3.2,0);
  	glEnd();

   //kotak kuning badan 1 depan
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(9.9,-1,2.1);
     	glVertex3f(9.9,7,2.1);
        glVertex3f(17.6,7,2.1);
        glVertex3f(17.6,-1,2.1);
  	glEnd();
  	
  	//kotak kuning badan 1 belakang
   glBegin(GL_POLYGON); 
        glColor3f(1.0,1.0,0.0); 
      	glVertex3f(9.9,-1,-4.1);
     	glVertex3f(9.9,7,-4.1);
        glVertex3f(17.6,7,-4.1);
        glVertex3f(17.6,-1,-4.1);
  	glEnd();
  	 	
     //kotak kuning badan 1 samping kiri
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.8,0.0); 
      	glVertex3f(9.9,-1,2.1);
     	glVertex3f(9.9,-1,-4.1);
        glVertex3f(9.9,7,-4.1);
        glVertex3f(9.9,7,2.1);
  	glEnd();
  	
  	//kotak kuning badan 1 samping kanan
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.8,0.0); 
      	glVertex3f(17.6,-1,2.1);
     	glVertex3f(17.6,-1,-4.1);
        glVertex3f(17.6,7,-4.1);
        glVertex3f(17.6,7,2.1);
  	glEnd();
  	
  	//kotak kuning badan 1 atas
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.7,0.0); 
      	glVertex3f(9.9,7,2.1);
     	glVertex3f(9.9,7,-4.1);
        glVertex3f(17.6,7,-4.1);
        glVertex3f(17.6,7,2.1);
  	glEnd();
  	
  	//kotak biru badan 1 dalem
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,1.0); 
      	glVertex3f(11,0,2.2);
     	glVertex3f(11,6,2.2);
        glVertex3f(16.5,6,2.2);
        glVertex3f(16.5,0,2.2);
  	glEnd();
  	
  	//kotak biru badan 1 dalem belakang
   glBegin(GL_POLYGON); 
        glColor3f(0.0,0.0,1.0); 
      	glVertex3f(11,0,-4.2);
     	glVertex3f(11,6,-4.2);
        glVertex3f(16.5,6,-4.2);
        glVertex3f(16.5,0,-4.2);
  	glEnd();
  	
   //kotak merah atas badan 1 depan
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.0,0.0); 
      	glVertex3f(10,7,2.1);
     	glVertex3f(9,8,2.1);
        glVertex3f(18.5,8,2.1);
        glVertex3f(17.5,7,2.1);
  	glEnd();
  	
  	//kotak merah atas badan 1 belakang
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.0,0.0); 
      	glVertex3f(10,7,-4.1);
     	glVertex3f(9,8,-4.1);
        glVertex3f(18.5,8,-4.1);
        glVertex3f(17.5,7,-4.1);
  	glEnd();
  	
  	//kotak merah atas badan 1 samping kiri
   glBegin(GL_POLYGON); 
        glColor3f(0.8,0.0,0.0); 
      	glVertex3f(10,7,2.1);
     	glVertex3f(9,8,2.1);
        glVertex3f(9,8,-4.1);
        glVertex3f(10,7,-4.1);
  	glEnd();
  	
  	//kotak merah atas badan 1 samping kanan
   glBegin(GL_POLYGON); 
        glColor3f(0.8,0.0,0.0); 
      	glVertex3f(17.5,7,2.1);
     	glVertex3f(18.5,8,2.1);
        glVertex3f(18.5,8,-4.1);
        glVertex3f(17.5,7,-4.1);
  	glEnd();
  	
	
	//kotak merah atas badan 1 atas
   glBegin(GL_POLYGON); 
        glColor3f(0.7,0.0,0.0); 
      	glVertex3f(9,8,2.1);
     	glVertex3f(9,8,-4.1);
        glVertex3f(18.5,8,-4.1);
        glVertex3f(18.5,8,2.1);
  	glEnd();  

   //kotak bawah depan
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.6,0.0); 
      	glVertex3f(9.9,-4.1,2.3);
     	glVertex3f(9.9,-0.9,2.3);
        glVertex3f(24.1,-0.9,2.3);
        glVertex3f(24.1,-4.1,2.3);
  	glEnd();
  	
  	//kotak bawah belakang
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.6,0.0); 
      	glVertex3f(9.9,-4.1,-4.3);
     	glVertex3f(9.9,-0.9,-4.3);
        glVertex3f(24.1,-0.9,-4.3);
        glVertex3f(24.1,-4.1,-4.3);
  	glEnd();
  	
  	//kotak bawah samping kiri
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.4,0.0); 
      	glVertex3f(9.9,-4.1,2.3);
     	glVertex3f(9.9,-4.1,-4.3);
        glVertex3f(9.9,-0.9,-4.3);
        glVertex3f(9.9,-0.9,2.3);
  	glEnd();
  	
  	//kotak bawah samping kanan
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.4,0.0); 
      	glVertex3f(24.1,-4.1,2.3);
     	glVertex3f(24.1,-4.1,-4.3);
        glVertex3f(24.1,-0.9,-4.3);
        glVertex3f(24.1,-0.9,2.3);
  	glEnd();
  	
  	//kotak bawah bagian bawah
   glBegin(GL_POLYGON); 
        glColor3f(1.0,0.3,0.0); 
      	glVertex3f(9.9,-4.1,2.3);
     	glVertex3f(9.9,-4.1,-4.3);
        glVertex3f(24.1,-4.1,-4.3);
        glVertex3f(24.1,-4.1,2.3);
  	glEnd();
  	
  	//roda 1
  	//roda hitam
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(-23, -3.5, 1.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
    
    //roda hitam belakang
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(-23, -3.5, -3.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(-23, -3.5, 3.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //roda putih belakang
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(-23, -3.5, -5.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    
   	//roda 2
  	//roda hitam
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(-19, -3.5, 1.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(-19, -3.5, 3.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
     //roda hitam belakang
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(-19, -3.5, -3.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih belakang
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(-19, -3.5, -5.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
   	//roda 3
  	//roda hitam
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(-11, -3.5, 1.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(-11, -3.5, 3.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //roda hitam belakang
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(-11, -3.5, -3.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih belakang 
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(-11, -3.5, -5.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
         
   	//roda 4
  	//roda hitam
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(-7, -3.5, 1.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(-7, -3.5, 3.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
        
    //roda hitam belakang
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(-7, -3.5, -3.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih belakang
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(-7, -3.5, -5.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
     
   	//roda 5
  	//roda hitam
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(1, -3.5, 1.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(1, -3.5, 3.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //roda hitam belakang
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(1, -3.5, -3.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih belakang
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(1, -3.5, -5.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
         
   	//roda 6
  	//roda hitam
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(5, -3.5, 1.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(5, -3.5, 3.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //roda hitam belakang
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(5, -3.5, -3.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih belakang
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(5, -3.5, -5.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //roda 7
  	//roda hitam
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(13.2, -3, 1.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1.3, 1.3, 1.3);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(13.2, -3, 3.0);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1.5, 1.5, 1.5);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //roda hitam belakang
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(13.2, -3, -3.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1.3, 1.3, 1.3);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih belakang
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(13.2, -3, -5.0);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1.5, 1.5, 1.5);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //roda 8
  	//roda hitam
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(17.5, -3.5, 1.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(17.5, -3.5, 3.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //roda hitam belakang
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(17.5, -3.5, -3.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih belakang
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(17.5, -3.5, -5.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //roda 9
  	//roda hitam
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(21, -3.5, 1.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(21, -3.5, 3.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();

    //roda hitam belakang
  	glColor3f(0,0,0);
  	glPushMatrix();
    glTranslatef(21, -3.5, -3.5);
    glRotatef(90.0, 1.0, 0.0, 2);
    glScalef(1, 1.1, 1);

    glutSolidSphere(2, 12, 50);
    glPopMatrix();
   
    //roda putih belakang
    glColor3f(1,1,1);
  	glPushMatrix();
    glTranslatef(21, -3.5, -5.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //==================================ASAP====================================
    
    //asap1.1
    glColor3f(0.6,0.6,0.6);
  	glPushMatrix();
    glTranslatef(22, 7, 0.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //asap1.2
    glColor3f(0.6,0.6,0.6);
  	glPushMatrix();
    glTranslatef(20.5, 7, 0.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //asap1.3
    glColor3f(0.6,0.6,0.6);
  	glPushMatrix();
    glTranslatef(21, 8, 0.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(1, 1.1, 1);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //asap2.1
     glColor3f(0.6,0.6,0.6);
  	glPushMatrix();
    glTranslatef(19, 11, 0.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(0.9, 0.9, 0.9);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //asap2.2
    glColor3f(0.6,0.6,0.6);
  	glPushMatrix();
    glTranslatef(18, 10, 0.0);
    glRotatef(90.0, 1.0, 0.0, 3);
    glScalef(0.9, 0.9, 0.9);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
   
    //asap2.3
     glColor3f(0.6,0.6,0.6);
  	glPushMatrix();
    glTranslatef(18, 11, 0.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(0.9, 0.9, 0.9);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
    //asap3.1
     glColor3f(0.6,0.6,0.6);
  	glPushMatrix();
    glTranslatef(16, 14, 0.0);
    glRotatef(90.0, 1.0, 0.0, 1);
     glScalef(0.8, 0.8, 0.8);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();
    
     //asap3.2
    glColor3f(0.6,0.6,0.6);
  	glPushMatrix();
    glTranslatef(15, 14.5, 0.0);
    glRotatef(90.0, 1.0, 0.0, 1);
     glScalef(0.8, 0.8, 0.8);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();

    //asap3.3
     glColor3f(0.6,0.6,0.6);
  	glPushMatrix();
    glTranslatef(16, 15, 0.0);
    glRotatef(90.0, 1.0, 0.0, 1);
    glScalef(0.8, 0.8, 0.8);
    
    glutSolidSphere(1, 12, 50);
    glPopMatrix();

                    
}









//=================================================================BANGUNAN=============================================================//



//material bangunan

void gedung(void) {
 
    //atap
    glPushMatrix();
    glScaled(0.8, 1.0, 1.15);
    glTranslatef(0.0, 2.4, 0.0);
    glRotated(45, 0, 1, 0);
    glRotated(-90, 1, 0, 0);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3d(1.0, 0.1, 0.1);
    glutSolidCone(5.2, 1.75, 4, 5);
    glPopMatrix();
  
    //atap kanan
    glPushMatrix();
    glScaled(0.78, 0.6, 0.91);
    glTranslatef(7.3, 2, 0.0);
    glRotated(45, 0, 1, 0);
    glRotated(-90, 1, 0, 0);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3d(1.0, 0.1, 0.1);
    glutSolidCone(5.8, 1.75, 4, 5);
    glPopMatrix();

    //atap kiri
    glPushMatrix();
    glScaled(0.78, 0.6, 0.91);
    glTranslatef(-7.3, 2, 0.0);
    glRotated(45, 0, 1, 0);
    glRotated(-90, 1, 0, 0);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3d(1.0, 0.1, 0.1);
    glutSolidCone(5.8, 1.75, 4, 5);
    glPopMatrix();

    //lantai utama
    glPushMatrix();
    glScaled(3.2, 0.1, 0.9);
    glTranslatef(0.7, -7.5,3);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(8);
    glPopMatrix();
    
    //lantai depan
    glPushMatrix();
    glScaled(4, 0.1, 0.1);
    glTranslatef(-0.2, -7.5,81);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(8);
    glPopMatrix();
    

    //lantai parkiran depan yg dikit
    glPushMatrix();
    glScaled(1.1, 0.1, 0.3);
    glTranslatef(-12.9, -7.5,17);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(8);
    glPopMatrix();

    //lantai parkiran samping yg dikit
    glPushMatrix();
    glScaled(0.2, 0.032, 1.3);
    glTranslatef(-90, -15.0,0.01);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(8);
    glPopMatrix();
    
    //lantai utama belakang
    glPushMatrix();
    glScaled(4.2, 0.08, 1.5);
    glTranslatef(-0.5, -11,0.1);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(8);
    glPopMatrix();
    
    //lantai kotak belakang
    glPushMatrix();
    glScaled(1.2, 0.08, 0.9);
    glTranslatef(-0.5, -11,-11);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(9);
    glPopMatrix();
    
    //lantai kotak parkiran
    glPushMatrix();
    glScaled(0.85, 0.08, 0.9);
    glTranslatef(-16.5, -10,0);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.5,0.5,0.5);  
    glutSolidCube(8);
    glPopMatrix();

   ///Dinding Kiri tengah
    glPushMatrix();
    glScaled(0.035, 0.7, 1.6);
    glTranslatef(-70.0, 1, 0.0);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);
    glutSolidCube(5.0);
    glPopMatrix();
  
  //Dinding Kanan tengah 
    glPushMatrix();
    glScaled(0.035, 0.7, 1.6);
    glTranslatef(70.0, 1, 0.0);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);  
    glutSolidCube(5.0);
    glPopMatrix();
    
    //Dinding Belakang tengah
    glPushMatrix();
    glScaled(1.015, 0.7, 0.07);
    glTranslatef(0, 1,-58);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);  
    glutSolidCube(5.0);
    glPopMatrix();
    
    //Dinding Depan tengah
    glPushMatrix();
    glScaled(1.015, 0.7, 0.035);
    glTranslatef(0, 1,116); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);   
    glutSolidCube(5.0);
    glPopMatrix();
    
        
    //Dinding Kanan belah kanan 
    glPushMatrix();
    glScaled(0.035, 0.35, 1.4);
    glTranslatef(247.5, 1, 0.0);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);  
    glutSolidCube(5.0);
    glPopMatrix();
    
    //Dinding Belakang belah kanan
    glPushMatrix();
    glScaled(1.25, 0.35, 0.07);
    glTranslatef(4.5, 1,-50);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);  
    glutSolidCube(5.0);
    glPopMatrix();
    
    //Dinding Depan belah kanan
    glPushMatrix();
    glScaled(1.25, 0.35, 0.035);
    glTranslatef(4.5, 1,100); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);   
    glutSolidCube(5.0);
    glPopMatrix();
    
    
    //Dinding Kiri belah kiri
    glPushMatrix();
    glScaled(0.035, 0.35, 1.4);
    glTranslatef(-247.5, 1, 0.0);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);  
    glutSolidCube(5.0);
    glPopMatrix();
    
    //Dinding Belakang belah kiri
    glPushMatrix();
    glScaled(1.25, 0.35, 0.07);
    glTranslatef(-4.5, 1,-50);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);  
    glutSolidCube(5.0);
    glPopMatrix();
    
    //Dinding Depan belah kiri
    glPushMatrix();
    glScaled(1.25, 0.35, 0.035);
    glTranslatef(-4.5, 1,100); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.4, 0.4, 0.4);   
    glutSolidCube(5.0);
    glPopMatrix();
    
    
    
    //Jendela tengah I
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(0.8, 2.45,85); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    
    //Jendela tengah II
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(-0.8, 2.45,85); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
     //Jendela tengah III
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(-2.4, 2.45,85); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela tengah IV
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(2.4, 2.45,85); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Pintu Tengah I
    glPushMatrix();
    glScaled(0.50, 0.7, 0.049);
    glTranslatef(0.9, 0.4,85); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.75);
    glPopMatrix();
    
     //Pintu Tengah I
    glPushMatrix();
    glScaled(0.50, 0.7, 0.049);
    glTranslatef(-0.9, 0.4,85); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.75);
    glPopMatrix();
    
     //Jendela samping pintu 1
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(2.3, 0.75,85); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
     
      //Jendela samping pintu II
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(-2.3, 0.75,85); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
     //Jendela dinding samping 1
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(-3.5, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping II
      glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(-4.6, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping III
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(-5.7, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping IV
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(-6.8, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping V
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(-7.9, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    
    
    //Jendela dinding samping 1
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(3.5, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping II
      glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(4.6, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping III
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(5.7, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping IV
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(6.8, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping V
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(7.9, 0.6,73); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();





// ===================== J E N D E L A  B E L A K A N G ========================


//Jendela tengah I
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(2.3, 2.45,-86); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    
    //Jendela tengah II
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(-2.3, 2.45,-86); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
            
    //Pintu Tengah I
    glPushMatrix();
    glScaled(0.50, 0.7, 0.049);
    glTranslatef(0.9, 0.4,-86); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.75);
    glPopMatrix();
    
     //Pintu Tengah II
    glPushMatrix();
    glScaled(0.50, 0.7, 0.049);
    glTranslatef(-0.9, 0.4,-86); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.75);
    glPopMatrix();
    
     //Jendela samping pintu 1
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(2.3, 0.75,-86); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
     
      //Jendela samping pintu II
    glPushMatrix();
    glScaled(0.7, 0.7, 0.049);
    glTranslatef(-2.3, 0.75,-86); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
     //Jendela dinding samping 1
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(-4.5, 0.6,-75); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping II
      glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(-6.6, 0.6,-75); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
              
    //Jendela dinding samping 1
    glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(4.5, 0.6,-75); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    
    //Jendela dinding samping II
      glPushMatrix();
    glScaled(1, 0.7, 0.049);
    glTranslatef(6.6, 0.6,-75); 
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.1, 0.1, 0.1);   
    glutSolidCube(1.0);
    glPopMatrix();
    

    //Tangga belakang
    glPushMatrix();
    glScaled(1, 0.2, 0.3);
    glTranslatef(0, -3,-15.5);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(3);
    glPopMatrix();


    glPushMatrix();
    glScaled(0.5, 0.01, 0.03);
    glTranslatef(0, -40,-171);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(3);
    glPopMatrix();

    glPushMatrix();
    glScaled(0.5, 0.01, 0.08);
    glTranslatef(0, -43,-64.5);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(3);
    glPopMatrix();

    glPushMatrix();
    glScaled(0.5, 0.01, 0.1);
    glTranslatef(0, -46,-52.5);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(3);
    glPopMatrix();

    glPushMatrix();
    glScaled(0.5, 0.01, 0.15);
    glTranslatef(0, -49,-35.5);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.95, 0.95, 0.95);  
    glutSolidCube(3);
    glPopMatrix();

    
}

















//========================JALAN==========================================//

void garisjalanvertikal(void) {
 
    glPushMatrix();
    glScaled(0.08, 0.005, 0.5);
    glTranslatef(-30.0, 2.45, 0.0);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 1.0, 1.0);
    glutSolidCube(5.0);
    glPopMatrix();
}


void garisjalanhorizontal(void) {
 
    glPushMatrix();
    glScaled(0.5, 0.01, 0.07);
    glTranslatef(1.0, 1.0, 0.0);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 1.0, 1.0);
    glutSolidCube(5.0);
    glPopMatrix();
}

//===========================TAMAN BELAKANG==================================//

void kursi(void){
    // Batang Tiang Kanan
    glPushMatrix();
    glScaled(0.06, 0.2,0.06);
   glTranslatef(43, 0,380.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1,1,1); 
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    
    // Batang Tiang Kiri
    glPushMatrix();
    glScaled(0.06, 0.2,0.06);
   glTranslatef(3, 0,380.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1,1,1); 
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    // Batang depan knan
    glPushMatrix();
    glScaled(0.06, 0.2,0.06);
   glTranslatef(43, 0,390.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1,1,1);
    glutSolidCube(5.0);
    glPopMatrix(); 
    
    // Batang Depan Kiri
    glPushMatrix();
    glScaled(0.06, 0.2,0.06);
   glTranslatef(3, 0,390.5); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1,1,1); 
    glutSolidCube(5.0);
    glPopMatrix();     

    // atas kursi
    glPushMatrix();
    glScaled(0.6, 0.05,0.3);
   glTranslatef(2.4,8,77); 
   glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
     glColor3f(1.0000, 0.5252, 0.0157);
    glutSolidCube(5.0);
    glPopMatrix();  
   
}

void TB(void) {
    
    //meja tengah
    glPushMatrix();
    glScaled(0.3, 0.2, 0.9);
    glTranslatef(-0.5, 0,-10);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.76,1.0,1.0);  
    glutSolidTorus(2,7,25,25);
    glPopMatrix();
}

void TBair(void) {
    
    //meja tengah
    glPushMatrix();
    glScaled(0.3, 0.2, 0.9);
    glTranslatef(-0.5, 0,-10);  
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.0,0.0,1.0);  
    glutSolidTorus(2,5,25,25);
    glPopMatrix();
}

//===========================TANAMAN=========================================//

//=======POHON=======//

void pohon(void){
//batang
GLUquadricObj *pObj;
pObj =gluNewQuadric();
gluQuadricNormals(pObj, GLU_SMOOTH);    
glPushMatrix();
glColor3ub(104,70,14);
glRotatef(270,1,0,0);
gluCylinder(pObj, 4, 0.7, 30, 25, 25);
glPopMatrix();
}


void ranting(void){
//ranting
GLUquadricObj *pObj;
pObj =gluNewQuadric();
gluQuadricNormals(pObj, GLU_SMOOTH); 
glPushMatrix();
glColor3ub(104,70,14);
glTranslatef(0,27,0);
glRotatef(330,1,0,0);
gluCylinder(pObj, 0.6, 0.1, 15, 25, 25);
glPopMatrix();

//daun
glPushMatrix();
glColor3ub(18,118,13);
glScaled(5, 5, 5);
glTranslatef(0,7,3);
glutSolidDodecahedron();
glPopMatrix();

}

// ==== terowongan =================================

void terowongan (void){
     
    glPushMatrix();
    glScaled(0.3, 0.05, 0.65);
    glTranslatef(-58.0, 53, -1);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 1.0, 1.0);
    glutSolidCube(5.0);
    glPopMatrix();
 
    glPushMatrix();
    glScaled(0.3, 0.5, 0.1);
    glTranslatef(-58.0, 3, 12);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 1.0, 1.0);
    glutSolidCube(5.0);
    glPopMatrix();
 
    glPushMatrix();
    glScaled(0.3, 0.5, 0.1);
    glTranslatef(-58.0, 3, -20);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 1.0, 1.0);
    glutSolidCube(5.0);
    glPopMatrix();
     
    glPushMatrix();
    glScaled(0.3, 0.5, 0.55);
    glTranslatef(-58.0, 2.3, -0.8);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(0.0, 0.0, 0.0);
    glutSolidCube(5.0);
    glPopMatrix();
     
}

// ========= awan ========================
void awan(void){
glPushMatrix(); 
glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
glColor3ub(153, 223, 255);
glutSolidSphere(10, 50, 50);
glPopMatrix();
glPushMatrix();
glTranslatef(10,0,1);
glutSolidSphere(5, 50, 50);
glPopMatrix();   
glPushMatrix();
glTranslatef(-2,6,-2);
glutSolidSphere(7, 50, 50);
glPopMatrix();   
glPushMatrix();
glTranslatef(-10,-3,0);
glutSolidSphere(7, 50, 50);
glPopMatrix();  
glPushMatrix();
glTranslatef(6,-2,2);
glutSolidSphere(7, 50, 50);
glPopMatrix();      
}     
 

// ============ R E L ==================

void rel (void){
    glPushMatrix();
    glScaled(8, 0.05, 0.02);
    glTranslatef(0.5, 3, -30);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 1.0, 1.0);
    glutSolidCube(5.0);
    glPopMatrix();
}

void relkecil (void){
    glPushMatrix();
    glScaled(0.03, 0.05, 0.25);
    glTranslatef(450, 3, -1);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 0.5, 0.0);
    glutSolidCube(5.0);
    glPopMatrix();
    
    glPushMatrix();
    glScaled(0.03, 0.05, 0.25);
    glTranslatef(470, 3, -1);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 0.5, 0.0);
    glutSolidCube(5.0);
    glPopMatrix();
    
    glPushMatrix();
    glScaled(0.03, 0.05, 0.25);
    glTranslatef(490, 3, -1);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 0.5, 0.0);
    glutSolidCube(5.0);
    glPopMatrix();
    
    glPushMatrix();
    glScaled(0.03, 0.05, 0.25);
    glTranslatef(510, 3, -1);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 0.5, 0.0);
    glutSolidCube(5.0);
    glPopMatrix();
    
    glPushMatrix();
    glScaled(0.03, 0.05, 0.25);
    glTranslatef(530, 3, -1);   
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glColor3f(1.0, 0.5, 0.0);
    glutSolidCube(5.0);
    glPopMatrix();
}








//==============================================================================

void display(void) {
	glClearStencil(0); //clear the stencil buffer
	glClearDepth(1.0f);
	glClearColor(0.0, 0.6, 0.8, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); //clear the buffers
	glLoadIdentity();
	gluLookAt(viewx, viewy, viewz, 0.0, 0.0, 5.0, 0.0, 1.0, 0.0);

	glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrain, 0.3f, 0.9f, 0.0f);
	glPopMatrix();

	glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrainTanah, 0.7f, 0.2f, 0.1f);
	glPopMatrix();

	glPushMatrix();

	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrainAir, 0.0f, 0.2f, 0.5f);
	glPopMatrix();
	
	//glBindTexture(GL_TEXTURE_3D, texture[0]);
	drawSceneTanah(_terrainJalan, 0.5f, 0.5f, 0.5f);
	glPopMatrix();







//=======memanggil void=====================//

//gedung
glPushMatrix();
glTranslatef(160,10,160); 
glScalef(5, 5, 5);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
gedung();
glPopMatrix();



//garis jalan vertikal
glPushMatrix();
glTranslatef(122.5,6.4,110); 
glScalef(5, 5, 5);
garisjalanvertikal();
glPopMatrix();

//garis jalan vertikal
glPushMatrix();
glTranslatef(124,6.4,90); 
glScalef(5, 5, 5);
garisjalanvertikal();
glPopMatrix();

//garis jalan horizontal
glPushMatrix();
glTranslatef(145,6.4,72); 
glScalef(5, 5, 5);
garisjalanhorizontal();
glPopMatrix();

//garis jalan horizontal
glPushMatrix();
glTranslatef(125,6.4,72); 
glScalef(5, 5, 5);
garisjalanhorizontal();
glPopMatrix();

//garis jalan horizontal
glPushMatrix();
glTranslatef(93,6.4,72); 
glScalef(5, 5, 5);
garisjalanhorizontal();
glPopMatrix();

//garis jalan horizontal
glPushMatrix();
glTranslatef(165,6.4,72); 
glScalef(5, 5, 5);
garisjalanhorizontal();
glPopMatrix();

//garis jalan horizontal
glPushMatrix();
glTranslatef(185,6.4,72); 
glScalef(5, 5, 5);
garisjalanhorizontal();
glPopMatrix();

//garis jalan horizontal
glPushMatrix();
glTranslatef(205,6.4,72); 
glScalef(5, 5, 5);
garisjalanhorizontal();
glPopMatrix();

//garis jalan diagonal
glPushMatrix();
glTranslatef(225,6.4,68); 
glScalef(5, 5, 5);
glRotatef(40,5,5,0);
garisjalanhorizontal();
glPopMatrix();

//garis jalan diagonal
glPushMatrix();
glTranslatef(245,6.4,58); 
glScalef(5, 5, 5);
glRotatef(40,5,5,0);
garisjalanhorizontal();
glPopMatrix();

//garis jalan diagonal
glPushMatrix();
glTranslatef(265,6.4,48); 
glScalef(5, 5, 5);
glRotatef(40,5,5,0);
garisjalanhorizontal();
glPopMatrix();

//garis jalan diagonal
glPushMatrix();
glTranslatef(285,6.4,38); 
glScalef(5, 5, 5);
glRotatef(40,5,5,0);
garisjalanhorizontal();
glPopMatrix();

//garis jalan diagonal
glPushMatrix();
glTranslatef(305,6.4,28); 
glScalef(5, 5, 5);
glRotatef(40,5,5,0);
garisjalanhorizontal();
glPopMatrix();


//========TAMAN BELAKANG========//

//TAMAN BELAKANG


//meja tengah
glPushMatrix();
glTranslatef(158,-35,115); 
glScalef(4, 4, 5);
glRotatef(90,4,0,0);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
TB();
glPopMatrix();

//air
glPushMatrix();
glTranslatef(158,10,72.5); 
glScalef(5, 5, 5);
glRotatef(190,3,0,0);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
TBair();
glPopMatrix();


//kursi
glPushMatrix();
glTranslatef(160,10,1); 
glScalef(4, 3, 4);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
kursi();
glPopMatrix();

//kursi 2
glPushMatrix();
glTranslatef(142,10,1); 
glScalef(4, 3, 4);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
kursi();
glPopMatrix();

//kursi 3
glPushMatrix();
glTranslatef(85,10,125); 
glRotatef(90,0,3,0);
glScalef(4, 3, 4);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
kursi();
glPopMatrix();

//kursi 4
glPushMatrix();
glTranslatef(85,10,110); 
glRotatef(90,0,3,0);
glScalef(4, 3, 4);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
kursi();
glPopMatrix();

//kursi 5
glPushMatrix();
glTranslatef(45,10,125); 
glRotatef(90,0,3,0);
glScalef(4, 3, 4);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
kursi();
glPopMatrix();

//kursi 6
glPushMatrix();
glTranslatef(45,10,110); 
glRotatef(90,0,3,0);
glScalef(4, 3, 4);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
kursi();
glPopMatrix();

//kursi kiri 2
//glPushMatrix();
//glTranslatef(160,10,160); 
//glScalef(5, 5, 5);
//glBindTexture(GL_TEXTURE_2D, texture[0]);
//kursikiri2();
//glPopMatrix();


//=====POHON====//

//pohon I
glPushMatrix();
glTranslatef(250,6.4,160);    
glScalef(0.4, 0.4, 0.4);
glRotatef(90,0,1,0);
pohon();

//ranting1 I
ranting();

//ranting2 I
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 I
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();


//pohon II
glPushMatrix();
glTranslatef(200,6.4,100);    
glScalef(0.4, 0.4, 0.4);
glRotatef(90,0,1,0);
pohon();

//ranting1 II
ranting();

//ranting2 II
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 II
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();


//pohon III
glPushMatrix();
glTranslatef(120,6.4,100);    
glScalef(0.4, 0.4, 0.4);
glRotatef(90,0,1,0);
pohon();

//ranting1 III
ranting();

//ranting2 III
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 III
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();


//pohon IV
glPushMatrix();
glTranslatef(80,6.4,120);    
glScalef(0.3, 0.3, 0.3);
glRotatef(90,0,1,0);
pohon();

//ranting1 IV
ranting();

//ranting2 IV
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 IV
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();


//pohon kecil I
glPushMatrix();
glTranslatef(190,6.4,110);    
glScalef(0.2, 0.2, 0.2);
glRotatef(90,0,1,0);
pohon();

//ranting
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();


//pohon kecil II
glPushMatrix();
glTranslatef(270,6.4,90);    
glScalef(0.2, 0.2, 0.2);
glRotatef(90,0,1,0);
pohon();

//ranting
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();


//pohon V
glPushMatrix();
glTranslatef(260,6.4,80);    
glScalef(0.3, 0.3, 0.3);
glRotatef(90,0,1,0);
pohon();

//ranting1 V
ranting();

//ranting2 V
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 V
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();

//pohon kecil III
glPushMatrix();
glTranslatef(60,6.4,130);    
glScalef(0.2, 0.2, 0.2);
glRotatef(90,0,1,0);
pohon();

//ranting
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();

//pohon kecil IV
glPushMatrix();
glTranslatef(60,6.4,160);    
glScalef(0.2, 0.2, 0.2);
glRotatef(90,0,1,0);
pohon();

//ranting
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();

//pohon kecil V
glPushMatrix();
glTranslatef(240,6.4,180);    
glScalef(0.2, 0.2, 0.2);
glRotatef(90,0,1,0);
pohon();

//ranting
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();

//pohon VI
glPushMatrix();
glTranslatef(60,6.4,180);    
glScalef(0.3, 0.3, 0.3);
glRotatef(90,0,1,0);
pohon();

//ranting1 VI
ranting();

//ranting2 VI
glPushMatrix();
glScalef(1.5, 1.5, 1.5);
glTranslatef(0,25,25);   
glRotatef(250,1,0,0);
ranting();
glPopMatrix();

//ranting3 VI
glPushMatrix();
glScalef(1.8, 1.8, 1.8);
glTranslatef(0,-6,21.5);   
glRotatef(-55,1,0,0);
ranting();
glPopMatrix();
glPopMatrix();






//KERETA API
glPushMatrix();
glTranslatef(150,10,196);    
glScalef(0.5,0.5,0.5);
glRotatef(0,0,1,0);
kereta();
glPopMatrix();



// ========== Terowongan =============

//terowongan jalan
glPushMatrix();
glTranslatef(145,6.4,72); 
glScalef(5, 5, 5);
terowongan();
glPopMatrix();

//terowongan kereta I
glPushMatrix();
glTranslatef(20,6.4,30); 
glScalef(5, 5, 5);
glRotatef(90,0,1,0);
terowongan();
glPopMatrix();

//terowongan kereta II
glPushMatrix();
glTranslatef(165,6.4,35); 
glScalef(5, 5, 5);
terowongan();
glPopMatrix();



// =========== awan ===================

//awan
glPushMatrix();
glTranslatef(-75, 110, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-45, 110, -115);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-50, 120, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-140, 90, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-155, 90, -115);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-130, 110, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-190, 110, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-175, 120, -115);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-200, 100, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-30, 110, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-35, 95, -115);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-20, 90, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(-80, 90, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();
  
glPushMatrix();
glTranslatef(220, 90, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(180, 90, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();


glPushMatrix();
glTranslatef(190, 110, -120);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();

glPushMatrix();
glTranslatef(125, 110, -115);
glScalef(1.8, 1.0, 1.0);  
awan();
glPopMatrix();


// R E L

//rel panjang
glPushMatrix();
glTranslatef(145,6.4,200); 
glScalef(5, 5, 5);
rel();
glPopMatrix();

//rel panjang
glPushMatrix();
glTranslatef(145,6.4,196); 
glScalef(5, 5, 5);
rel();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(0,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(15,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(30,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(45,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(60,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(75,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(90,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(105,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(120,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(135,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(150,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(165,6.4,196); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();






//rel panjang
glPushMatrix();
glTranslatef(145,6.4,38); 
glScalef(5, 5, 5);
rel();
glPopMatrix();

//rel panjang
glPushMatrix();
glTranslatef(145,6.4,34); 
glScalef(5, 5, 5);
rel();
glPopMatrix();

//rel panjang
glPushMatrix();
glTranslatef(145,6.4,34); 
glScalef(5, 5, 5);
rel();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(0,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(15,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(30,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(45,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(60,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(75,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(90,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(105,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(120,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(135,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(150,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();

//rel
glPushMatrix();
glTranslatef(165,6.4,34); 
glScalef(5, 5, 5);
relkecil();
glPopMatrix();



//==============================================================================

	glutSwapBuffers();
	glFlush();
	rot++;
	angle++;

}

void init(void) {
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glDepthFunc(GL_LESS);
	glEnable(GL_NORMALIZE);
	glEnable(GL_COLOR_MATERIAL);
	glDepthFunc(GL_LEQUAL);
	glShadeModel(GL_SMOOTH);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_CULL_FACE);

	_terrain = loadTerrain("heightmap.bmp", 70);
	_terrainTanah = loadTerrain("heightmapTanah.bmp", 20);
	_terrainAir = loadTerrain("heightmapAir.bmp", -15);
	_terrainJalan = loadTerrain("heightmapJalan.bmp", -13);


	//binding texture

Images *image1 = loadTexture();

if (image1 == NULL) {
		printf("Image was not returned from loadTexture\n");
		exit(0);
	}
	
glPixelStorei(GL_UNPACK_ALIGNMENT, 1);


	// Generate texture/ membuat texture
	glGenTextures(1, texture);


	
//------------tekstur rumah---------------

	//binding texture untuk membuat texture 2D
	glBindTexture(GL_TEXTURE_2D, texture[0]);


    
	//menyesuaikan ukuran textur ketika image lebih besar dari texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); //
	//menyesuaikan ukuran textur ketika image lebih kecil dari texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //

	glTexImage2D(GL_TEXTURE_2D, 0, 3, image1->sizeX, image1->sizeY, 0, GL_RGB,
			GL_UNSIGNED_BYTE, image1->data);
 // texture
}



static void kibor(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_HOME:
		viewy++;
		break;
	case GLUT_KEY_END:
		viewy--;
		break;
	case GLUT_KEY_UP:
		viewz--;
		break;
	case GLUT_KEY_DOWN:
		viewz++;
		break;

	case GLUT_KEY_RIGHT:
		viewx++;
		break;
	case GLUT_KEY_LEFT:
		viewx--;
		break;

	case GLUT_KEY_F1: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	case GLUT_KEY_F2: {
		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient2);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse2);
		glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
	}
		;
		break;
	default:
		break;
	}
}

void keyboard(unsigned char key, int x, int y) {
	if (key == 'd') {

		spin = spin - 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'a') {
		spin = spin + 1;
		if (spin > 360.0)
			spin = spin - 360.0;
	}
	if (key == 'q') {
		viewz++;
	}
	if (key == 'e') {
		viewz--;
	}
	if (key == 's') {
		viewy--;
	}
	if (key == 'w') {
		viewy++;
	}
}

void reshape(int w, int h) {
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (GLfloat) w / (GLfloat) h, 0.1, 1000.0);
	glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_STENCIL | GLUT_DEPTH); //add a stencil buffer to the window
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Stasiun Kereta 3D");
	init();

	glutDisplayFunc(display);
	glutIdleFunc(display);
	glutReshapeFunc(reshape);
	glutSpecialFunc(kibor);

	glutKeyboardFunc(keyboard);

	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);

	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT, GL_SHININESS, high_shininess);
	glColorMaterial(GL_FRONT, GL_DIFFUSE);

	glutMainLoop();
	return 0;
}
